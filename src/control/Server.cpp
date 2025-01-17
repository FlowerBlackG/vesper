// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Control Server
 * 
 * 创建于 2024年3月6日 上海市嘉定区安亭镇
 */

#include "./Server.h"
#include "../log/Log.h"
#include "./Protocols.h"
#include "../config.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <memory>
#include <unistd.h>
#include <map>
#include <set>

using namespace std;

namespace vesper::control {

const int SOCKET_DATA_BUF_SIZE = 16 * 1024;

Server::~Server() {
    this->clear();
}

static void clearRunOptionsResult(Server::RunOptions& options) {
    auto& res = options.result;

    res.msg.clear();
    res.code = 0;
    res.serverLaunchedSignal.try_acquire();
}

int Server::run() {
    clearRunOptionsResult(this->options);

    // init domain socket

    int listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd < 0) {
        const char* errMsg = "failed to create domain socket.";
        options.result.code = -1;
        options.result.msg = errMsg;
        LOG_ERROR(errMsg);
        return options.result.code;
    }

    sockaddr_un sockServer {0};
    sockServer.sun_family = AF_UNIX;
    strcpy(sockServer.sun_path, options.socketPath.c_str());
    unlink(options.socketPath.c_str());

    int size = offsetof(sockaddr_un, sun_path) + options.socketPath.length();

    if ( bind(listenFd, (sockaddr*) &sockServer, size) < 0 ) {
        options.result.code = -1;
        options.result.msg = "failed to bind domain socket: ";
        options.result.msg += options.socketPath;
        LOG_ERROR(options.result.msg);
        close(listenFd);
        return options.result.code;
    }

    if ( listen(listenFd, 20) < 0 ) {
        options.result.code = -1;
        options.result.msg = "failed to listen domain socket!";
        LOG_ERROR(options.result.msg)
        close(listenFd);
        return options.result.code;
    }


    this->socketListenFd = listenFd;


    // event loop

    options.result.code = 0;
    options.result.serverLaunchedSignal.release();

    signal(SIGPIPE, SIG_IGN);
    this->systemRunning = true;

    int clientRes;
    while ( (clientRes = acceptClient()) > 0 )
        ;
    

    // clean up

    this->clear();
    return options.result.code;
}

void Server::clear() {

    if (this->socketListenFd != -1) {
        close(this->socketListenFd);
        this->socketListenFd = -1;
    }

    unlink(options.socketPath.c_str());
}


void Server::terminate() {
    this->systemRunning = false;
    if (this->socketListenFd != -1) {
        shutdown(this->socketListenFd, SHUT_RDWR);
    }
}


/* ------------ protected methods ------------ */


static int readNBytesFromSocket(int connFd, int n, char* buf) {
    int totalBytesRead = 0;
    char* dataPtr = buf;
    while (totalBytesRead < n) {
        int bytes = read(connFd, dataPtr, n - totalBytesRead);
        if (bytes < 0) {
            LOG_ERROR("read error! nBytes is ", n);
            return -2;
        } else if (bytes == 0) {
            LOG_ERROR("unexpected EOF from socket.");
            return -3;
        }

        totalBytesRead += bytes;
        dataPtr += bytes;
    }

    return 0;
}


/**
 * 
 * 
 * @param connFd 
 * @param code 0 表示正常。
 * @param msg 
 */
static void sendResponse(int connFd, uint32_t code, const string& msg) {
    protocol::Response response;
    response.code = code;
    response.msg = msg;

    stringstream s(ios::in | ios::out | ios::binary);
    response.encode(s);

    string str = s.str();
    write(connFd, str.c_str(), str.length());
}


int Server::acceptClient() {
    sockaddr_un client;
    socklen_t clientLen = sizeof(client);
    int connFd = accept(this->socketListenFd, (sockaddr*) &client, &clientLen);

    if (connFd < 0) {

        if (!systemRunning) {
            return 0;
        }

        LOG_WARN("socket connection failed.");
        return 1;
    }

    int resCode = 0;

    do {
        // 读入 header

        auto dataPtr = socketDataBuf.data();

        if (readNBytesFromSocket(connFd, protocol::HEADER_LEN, dataPtr)) {
            const char* err = "failed to read header!";
            LOG_ERROR(err);
            sendResponse(connFd, 2, err);
            resCode = 2;
            break;
        }

        if (strncmp(dataPtr, protocol::MAGIC_STR, 4)) {
            const char* err = "magic mismatched!";
            LOG_ERROR(err);
            sendResponse(connFd, 3, err);
            resCode = 3;
            break;
        }
        dataPtr += 4;

        uint32_t type = be32toh(*(uint32_t*) dataPtr);
        dataPtr += 4;

        uint64_t length = be64toh(*(uint64_t*) dataPtr);
        dataPtr += 8;

        if (length + protocol::HEADER_LEN > socketDataBuf.capacity())
            socketDataBuf.reserve(length + protocol::HEADER_LEN);

        dataPtr = socketDataBuf.data();

        if (readNBytesFromSocket(connFd, length, dataPtr + protocol::HEADER_LEN)) {
            const char* err = "failed to read body!";
            LOG_ERROR(err);
            sendResponse(connFd, 5, err);
            resCode = 5;
            break;
        }
    
        protocol::Base* protocol = protocol::decode(dataPtr, type, length + protocol::HEADER_LEN);

        if (protocol == nullptr) {
            const char* err = "failed to parse protocol!";
            LOG_ERROR(err);
            sendResponse(connFd, 7, err);
            resCode = 6;
            break;
        }

        resCode = processProtocol(protocol, connFd);

    } while (0); // end of do-while(0)

    close(connFd);
    return resCode;
}


int Server::processProtocol(protocol::Base* protocol, int connFd) {
    auto protocolType = protocol->getType();

#define TRY_PROCESS(ProtoType) \
    case protocol::ProtoType::typeCode: { \
        return process ## ProtoType((protocol::ProtoType*) protocol, connFd); \
    }

    switch (protocolType) {
        VESPER_CTRL_EXPAND_RECV_PROTOS(TRY_PROCESS)

        default: {
            LOG_ERROR("protocol type unrecognized: ", protocolType);
            return 1;
        }
    }

#undef TRY_PROCESS
    
}


/* ------ protocol processors ------ */

// 函数返回：
//   > 0 : *正常* 返回，*愿意* 接收新报文
//   = 0 : *正常* 返回，*拒绝* 接收新报文
//   < 0 : *异常* 返回，*拒绝* 接收新报文


int Server::processTerminateVesper(protocol::TerminateVesper*, int connFd) {

    if (options.hooks.terminateVesper) {
        sendResponse(connFd, 0, "success");
        options.hooks.terminateVesper();
    } else {
        LOG_ERROR("hook: terminateVesper not registered!");
        sendResponse(connFd, -1, "hook not registered.");
    }

    return 0;
}


int Server::processGetVNCPort(protocol::GetVNCPort*, int connFd) {

    if (options.hooks.getVNCPort) {
        sendResponse(connFd, 0, to_string(options.hooks.getVNCPort()));
    } else {
        sendResponse(connFd, -1, "VNC is not active");
    }

    return 1;
}



int Server::processGetVNCPassword(protocol::GetVNCPassword*, int connFd) {

    if (options.hooks.getVNCPassword) {
        sendResponse(connFd, 0, options.hooks.getVNCPassword());
    } else {
        sendResponse(connFd, -1, "VNC is not active");
    }
    

    return 1;
}


int Server::processGetVesperVersion(protocol::GetVesperVersion*, int connFd) {
    stringstream ss;

    ss << VESPER_VERSION_NAME << ","
        << VESPER_VERSION_CODE << ","
        << VESPER_BUILD_TIME_ISO8601;

    sendResponse(connFd, 0, ss.str());

    return 1;
}


} // namespace vesper::control
