// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Entry
 *
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#include "config.h"
#include "log/Log.h"
#include "desktop/server/Server.h"

#include "./utils/wlroots-cpp.h"

#include <iostream>
#include <iomanip>
#include <sstream>

#include <unistd.h>

#include <wlr/version.h>
#include <pixman-version.h>

#include <rfb/rfb.h>

using namespace std;
using namespace vesper;

static void printVersion() {
    LOG_INFO(
        "vesper version : ", 
        VESPER_VERSION_NAME, 
        " (", VESPER_VERSION_CODE,")"
    );

    LOG_INFO(
        "wlroots version: ",
        WLR_VERSION_STR
    );

    LOG_INFO(
        "pixman version : ",
        PIXMAN_VERSION_STRING
    );
}

int main(int argc, const char* argv[]) {

    printVersion();

    LOG_INFO("creating desktop server")

    // ------ todo ------
/*
    int8_t* buf = new int8_t[1280*720*4];
    rfbScreenInfoPtr server = rfbGetScreen(0, nullptr, 1280, 720, 8, 3, 4);
    server->desktopName = "vnc test";
    server->frameBuffer = (char*) buf;
    server->alwaysShared = true;
    

    rfbInitServer(server);


    for (int i = 0; i < 1280; i++) {
        for (int j = 0; j < 720; j++) {
            ((int*) buf)[i * 720 + j] = 0xff00ff;
        }
    }
    

    while (1) {
        rfbMarkRectAsModified(server, 0, 0, 1280, 720);
        rfbProcessEvents(server, 1000000);
    }

    rfbShutdownServer(server, true);


    delete[] buf;*/

    // ------ todo ------
#if 1 // todo    
    desktop::server::Server server;
    server.run();
#endif
    LOG_INFO("bye~");
    return 0;
}
