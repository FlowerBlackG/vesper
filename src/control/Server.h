// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Control Server
 * 
 * 创建于 2024年3月6日 上海市嘉定区安亭镇
 */

#pragma once

#include <semaphore>
#include <string>
#include <functional>

#include "./Protocols.h"

namespace vesper::control {


class Server {

public:
    Server() {};
    ~Server();

    struct RunOptions {

        /**
         * control 模块放置 domain socket 的路径。
         * 
         * 例如：
         *     /run/user/1000/vesper/ctrl.sock
         * 
         * 要求文件夹存在并且本程序拥有读写权限。
         */
        std::string socketPath;

        struct {
            std::function<void ()> terminateVesper;
            std::function<const int ()> getVNCPort;
            std::function<const std::string& ()> getVNCPassword;
        } hooks = {0};

        struct {
            std::binary_semaphore serverLaunchedSignal {0};
            int code;
            std::string msg;
        } result;
    } options;

    int run();

    void clear();

    void terminate();


protected:

    /**
     * 
     * 
     * 
     * @return 异常退出时，返回负数；
     *         正常退出时，如果希望结束监听，返回0；否则返回正数。
     */
    int acceptClient();

    int processProtocol(protocol::Base* protocol, int connFd);

    /* ------ protocol processors ------ */

    int processTerminateVesper(protocol::TerminateVesper*, int connFd);
    int processGetVNCPort(protocol::GetVNCPort*, int connFd);
    int processGetVNCPassword(protocol::GetVNCPassword*, int connFd);
    int processGetVesperVersion(protocol::GetVesperVersion*, int connFd);

protected:
    char* socketDataBuf = nullptr;
    int socketListenFd = -1;

    bool systemRunning;

};


} // namespace vesper::control
