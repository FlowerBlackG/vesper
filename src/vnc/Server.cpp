// SPDX-License-Identifier: MulanPSL-2.0

/*
 * VNC 服务
 *
 * 创建于 2024年2月13日 北京市海淀区
 */

#include "./Server.h"

using namespace std;

namespace vesper::vnc {
    

static void mouseEventBridge(int buttonMask, int x, int y, rfbClientPtr cl) {
    auto* p = (Server*) cl->screen->screenData;
    p->mouseEventHandler(buttonMask, x, y, cl);
}

static void keyboardEventBridge(rfbBool down, rfbKeySym keySym, rfbClientPtr cl) {
    auto* p = (Server*) cl->screen->screenData;
    p->keyboardEventHandler(down, keySym, cl);
}


Server::~Server() {
    this->clear();
}

static void clearRunOptionsResult(Server::RunOptions& options) {
    auto& res = options.result;

    res.msg.clear();
    res.serverLaunchedSignal.try_acquire();
}

int Server::run() {

    clearRunOptionsResult(this->options);

    // prepare

    this->rfbServer = rfbGetScreen(
        nullptr, nullptr,
        options.screenBuffer.width,
        options.screenBuffer.height,
        8, 3, 4 
    );

    rfbServer->frameBuffer = options.screenBuffer.getBuffer(); // todo
    rfbServer->alwaysShared = true;
    rfbServer->serverFormat = {
        .bitsPerPixel = 32,
        .depth = 32,
        .bigEndian = false,
        .trueColour = true,
        .redMax = 0xFF,
        .greenMax = 0xFF,
        .blueMax = 0xFF,
        .redShift = 16,
        .greenShift = 8,
        .blueShift = 0,
    };

    rfbServer->ptrAddEvent = mouseEventBridge;
    rfbServer->kbdAddEvent = keyboardEventBridge;
    rfbServer->screenData = this;

    rfbInitServer(rfbServer);


    // event loop

    options.result.code = 0;
    options.result.serverLaunchedSignal.release();

    while (true) {
        rfbMarkRectAsModified(rfbServer, 0, 0, options.screenBuffer.width, options.screenBuffer.height);
        rfbProcessEvents(rfbServer, 50000);
    }

    // clean up

    this->clear();

    return options.result.code;
}

void Server::clear() {
    if (this->rfbServer) {
        rfbShutdownServer(rfbServer, true);
        rfbScreenCleanup(rfbServer);
        this->rfbServer = nullptr;
    }
}


void Server::mouseEventHandler(int buttonMask, int x, int y, rfbClientPtr cl) {
    auto& handler = options.eventHandlers.mouse.motion;
    auto& screenBufOpts = options.screenBuffer;
    if (handler) {
        
        // todo
        static int prevX = -1, prevY = -1;
        int thisDX, thisDY;
        if (prevX < 0 || prevY < 0) {
            thisDX = thisDY = 0;
            prevX = x;
            prevY = y;
        } else {
            thisDX = x - prevX;
            prevX = x;
            thisDY = y - prevY;
            prevY = y;
        }

        handler(
            true, double(x) / screenBufOpts.width, double(y) / screenBufOpts.height,
            true, thisDX, thisDY
        );
    }
}

void Server::keyboardEventHandler(rfbBool down, rfbKeySym keySym, rfbClientPtr cl) {
    // todo
}

}
