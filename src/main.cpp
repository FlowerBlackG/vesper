// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Entry
 *
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <semaphore>

#include <unistd.h>

#include <wlr/version.h>

#include <rfb/rfb.h>

#include "config.h"
#include "log/Log.h"
#include "desktop/server/Server.h"
#include "vnc/Server.h"

#include "./utils/wlroots-cpp.h"
#include "./common/MouseButton.h"

#include <pixman-1/pixman-version.h>

using namespace std;
using namespace vesper;
using namespace vesper::common;

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

    options.backend.headless = 0;
    options.backend.virtualOutput.add = true;
    options.backend.virtualOutput.width = 1280;
    options.backend.virtualOutput.height = 720;
    options.renderer.pixman = true;

    options.launch.apps.push_back("konsole");
    //options.launch.apps.push_back("LIBGL_ALWAYS_SOFTWARE=1 kgx");

    options.output.alwaysRenderEntireScreen = true;
    options.output.exportScreenBuffer = true;
    options.output.forceRenderSoftwareCursor = 1;

}

void vncSetOptions(vnc::Server& vnc) {
    auto& options = vnc.options;
    options.screenBuffer.width = 1280;
    options.screenBuffer.height = 720;
}

int main(int argc, const char* argv[]) {

    printVersion();

    LOG_INFO("creating desktop server")

  

    char* buf = new char[1280 * 720 * 4 * 10];

    desktop::server::Server desktop;
    auto& desktopResult = desktop.options.result;
    serverSetOptions(desktop);
    desktop.options.output.exportScreenBufferDest = buf;
    
    thread desktopThread([&desktop] () {
        int res = desktop.run();
        LOG_INFO("desktop server exited with code: ", res);
    });
    
    desktopResult.serverLaunchedSignal.acquire();
    
    if (desktopResult.code) {
        LOG_ERROR("server failed with code: ", desktopResult.code);
        return -1;
    }

    vnc::Server vnc;
    auto& vncResult = vnc.options.result;
    vncSetOptions(vnc);
    vnc.options.screenBuffer.getBuffer = [&buf] () {
        return buf;
    };
    vnc.options.eventHandlers.mouse.motion = [&desktop] (
        bool absolute, double absoluteX, double absoluteY,
        bool delta, int deltaX, int deltaY
    ) {
        desktop.moveCursorAsyncArgsMutex.acquire();
        auto& args = desktop.moveCursorAsyncArgs;
        args.absolute = absolute;
        args.absoluteX = absoluteX;
        args.absoluteY = absoluteY;
        args.delta = delta;
        args.deltaX = deltaX;
        args.deltaY = deltaY;
        desktop.moveCursorAsync();
        desktop.moveCursorAsyncArgsMutex.release();
    };

    vnc.options.eventHandlers.mouse.button = [&desktop] (
        bool press, MouseButton button
    ) {
        desktop.pressMouseButtonAsyncArgsMutex.acquire();
        auto& args = desktop.pressMouseButtonAsyncArgs;
        args.press = press;
        args.button = button;
        desktop.pressMouseButtonAsync();
        desktop.pressMouseButtonAsyncArgsMutex.release();
    };

    vnc.options.eventHandlers.mouse.axis = [&desktop] (
        bool vertical, double delta, int32_t deltaDiscrete
    ) {
        desktop.scrollAsyncArgsMutex.acquire();
        auto& args = desktop.scrollAsyncArgs;
        args.vertical = vertical;
        args.delta = delta;
        args.deltaDiscrete = deltaDiscrete;
        desktop.scrollAsync();
        desktop.scrollAsyncArgsMutex.release();
    };

    vnc.options.eventHandlers.keyboard.key = [&desktop] (
        bool pressed, xkb_keysym_t keysym
    ) {
        desktop.keyboardInputAsyncArgsMutex.acquire();
        auto& args = desktop.keyboardInputAsyncArgs;
        args.keysym = keysym;
        args.pressed = pressed;
        desktop.keyboardInputAsync();
        desktop.keyboardInputAsyncArgsMutex.release();
    };

    thread vncThread([&vnc] () {
        int res = vnc.run();
        LOG_INFO("vnc server exited with code: ", res);
    });

    vncThread.join();
    desktopThread.join();




    delete[] buf;


    LOG_INFO("bye~");
    return 0;
}
