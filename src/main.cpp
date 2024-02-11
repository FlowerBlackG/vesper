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
#include <thread>
#include <semaphore>

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

void serverSetOptions(desktop::server::Server& server) {
    auto& options = server.options;

    options.backend.headless = false;
    options.renderer.pixman = true;

    options.launch.apps.push_back("konsole");
    options.launch.apps.push_back("LIBGL_ALWAYS_SOFTWARE=1 kgx");

    options.output.alwaysRenderEntireScreen = true;
    options.output.exportScreenBuffer = true;

}

int main(int argc, const char* argv[]) {

    printVersion();

    LOG_INFO("creating desktop server")

#if 1 // todo    

char* buf = new char[1280 * 720 * 4];

    desktop::server::Server desktop;
    auto& desktopResult = desktop.options.result;
    serverSetOptions(desktop);
    desktop.options.output.exportScreenBufferDest = buf;
    
    thread desktopThread([&desktop] () {
        int res = desktop.run();
    });
    
    desktopResult.serverLaunchedSignal.acquire();
    
    if (desktopResult.code) {
        LOG_ERROR("server failed with code: ", desktopResult.code);
        return -1;
    }



#endif
    // ------ todo ------
#if 1

    rfbScreenInfoPtr server = rfbGetScreen(0, nullptr, 1280, 720, 8, 3, 4);
    server->desktopName = "vnc test";
    server->frameBuffer =  buf;
    server->alwaysShared = true;
    server->serverFormat = {
        .bitsPerPixel = 32,		

        .depth = 32,		

        .bigEndian = 0,		/* True if multi-byte pixels are interpreted
                    as big endian, or if single-bit-per-pixel
                    has most significant bit of the byte
                    corresponding to first (leftmost) pixel. Of
                    course this is meaningless for 8 bits/pix */

        .trueColour = true,		/* If false then we need a "colour map" to
                    convert pixels to RGB.  If true, xxxMax and
                    xxxShift specify bits used for red, green
                    and blue */

        /* the following fields are only meaningful if trueColour is true */

        .redMax = 0xFF,		/* maximum red value (= 2^n - 1 where n is the
                    number of bits used for red). Note this
                    value is always in big endian order. */

        .greenMax = 0xFF,		/* similar for green */

        .blueMax = 0xFF,		/* and blue */

        .redShift = 16,		/* number of shifts needed to get the red
                    value in a pixel to the least significant
                    bit. To find the red value from a given
                    pixel, do the following:
                    1) Swap pixel value according to bigEndian
                        (e.g. if bigEndian is false and host byte
                        order is big endian, then swap).
                    2) Shift right by redShift.
                    3) AND with redMax (in host byte order).
                    4) You now have the red value between 0 and
                        redMax. */

        .greenShift = 8,		/* similar for green */

        .blueShift = 0,		/* and blue */
    };

    rfbInitServer(server);


    for (int i = 0; i < 1280; i++) {
        for (int j = 0; j < 720; j++) {
            ((int*) buf)[i * 720 + j] = 0xff00ff;
        }
    }
    

    while (1) {
        rfbMarkRectAsModified(server, 0, 0, 1280, 720);
        rfbProcessEvents(server, 50000);
    }

    rfbShutdownServer(server, true);


    delete[] buf;
#endif
    // ------ todo ------

    LOG_INFO("bye~");
    return 0;
}
