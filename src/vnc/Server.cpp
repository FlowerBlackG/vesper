// SPDX-License-Identifier: MulanPSL-2.0

/*
 * VNC 服务
 *
 * 创建于 2024年2月13日 北京市海淀区
 */

#include "./Server.h"
#include <xkbcommon/xkbcommon.h>

using namespace std;

using namespace vesper::common;

namespace vesper::vnc {
    

Server::~Server() {
    this->clear();
}

static void clearRunOptionsResult(Server::RunOptions& options) {
    auto& res = options.result;

    res.msg.clear();
    res.serverLaunchedSignal.try_acquire();
}

int Server::run() {

    auto& opts = this->options;
    clearRunOptionsResult(opts);

    // prepare

    this->rfbServer = rfbGetScreen(
        nullptr, nullptr,
        options.screenBuffer.width,
        options.screenBuffer.height,
        8, 3, 4 
    );

    int framebufSize = opts.screenBuffer.width * opts.screenBuffer.height * 4;

    this->framebufferFallback = new (nothrow) char [framebufSize];
    if (this->framebufferFallback == nullptr) {
        const char* err = "failed to allocate framebuffer fallback!";
        LOG_ERROR(err);
        opts.result.msg = err;
        opts.result.code = -1;
        this->clear();
        return opts.result.code;
    }

    rfbServer->frameBuffer = nullptr;
    
    if (opts.net.port >= 0) {
        rfbServer->port = opts.net.port;
    }

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

    if (opts.auth.password != "") {
        rfbEncryptAndStorePasswd(
            (char*) opts.auth.password.c_str(), 
            (char*) opts.auth.libvncserverPasswdFile.c_str()
        );

        rfbServer->authPasswdData = (void*) opts.auth.libvncserverPasswdFile.c_str();
    }

    rfbServer->ptrAddEvent = [] (int buttonMask, int x, int y, rfbClientPtr cl) {
        auto* p = (Server*) cl->screen->screenData;
        p->mouseEventHandler(buttonMask, x, y, cl);
    };
    
    rfbServer->kbdAddEvent = [] (rfbBool down, rfbKeySym keySym, rfbClientPtr cl) {
        auto* p = (Server*) cl->screen->screenData;
        p->keyboardEventHandler(down, keySym, cl);
    };

    rfbServer->screenData = this;
    rfbServer->desktopName = "vesper remote";

    rfbInitServer(rfbServer);


    // event loop

    options.result.code = 0;
    options.result.serverLaunchedSignal.release();

    this->systemRunning = true;

    while (systemRunning) {
        bool usingFramebufFallback = rfbServer->frameBuffer == framebufferFallback;

        if (!usingFramebufFallback && rfbServer->frameBuffer && options.screenBuffer.recycleBuffer) {
            options.screenBuffer.recycleBuffer(rfbServer->frameBuffer);
        }

        if (options.screenBuffer.getBuffer) {
            rfbServer->frameBuffer = (char*) options.screenBuffer.getBuffer();
        } else {
            rfbServer->frameBuffer = nullptr;
        }

        if (rfbServer->frameBuffer == nullptr) {
            rfbServer->frameBuffer = this->framebufferFallback;
        }

        rfbMarkRectAsModified(rfbServer, 0, 0, options.screenBuffer.width, options.screenBuffer.height);
        rfbProcessEvents(rfbServer, 50000);
    }

    // clean up

    this->clear();

    return options.result.code;
}

void Server::clear() {
    if (this->framebufferFallback) {
        delete[] this->framebufferFallback;
        this->framebufferFallback = nullptr;
    }

    if (this->rfbServer) {
        rfbShutdownServer(rfbServer, true);
        rfbScreenCleanup(rfbServer);
        this->rfbServer = nullptr;
    }

    mouseData.prevX = mouseData.prevY = -1;
    mouseData.prevButtonMask = 0;
}


void Server::terminate() {
    this->systemRunning = false;
}


void Server::mouseEventHandler(int buttonMask, int x, int y, rfbClientPtr cl) {
    auto& motionHandler = options.eventHandlers.mouse.motion;
    auto& buttonHandler = options.eventHandlers.mouse.button;
    auto& axisHandler = options.eventHandlers.mouse.axis;
    auto& screenBufOpts = options.screenBuffer;
    
    const int LEFT_MASK = 1 << 0;
    const int MIDDLE_MASK = 1 << 1;
    const int RIGHT_MASK = 1 << 2;
    const int WHEEL_UP_MASK = 1 << 3;
    const int WHEEL_DOWN_MASK = 1 << 4;

    // 左中右三个按键

    if (mouseData.prevButtonMask != buttonMask) {
        auto diff = buttonMask ^ mouseData.prevButtonMask;

        if (buttonHandler) {
            if (diff & LEFT_MASK) {
                buttonHandler(buttonMask & LEFT_MASK, MouseButton::LEFT);
            }

            if (diff & MIDDLE_MASK) {
                buttonHandler(buttonMask & MIDDLE_MASK, MouseButton::MIDDLE);
            }

            if (diff & RIGHT_MASK) {
                buttonHandler(buttonMask & RIGHT_MASK, MouseButton::RIGHT);
            }
        }
    }

    // 滚轮

    if (axisHandler) {
        if (buttonMask & WHEEL_UP_MASK) {

            axisHandler(true, -15, -120);
        
        } else if (buttonMask & WHEEL_DOWN_MASK) {

            axisHandler(true, 15, 120);

        }
    }

    mouseData.prevButtonMask = buttonMask;

    // motion

    if (x != mouseData.prevX || y != mouseData.prevY) {
        int deltaXAbs = x;
        if (mouseData.prevX >= 0) {
            deltaXAbs -= mouseData.prevX;
        }
        int deltaYAbs = y;
        if (mouseData.prevY >= 0) {
            deltaYAbs -= mouseData.prevY;
        }
        mouseData.prevX = x;
        mouseData.prevY = y;

        if (motionHandler) {
            motionHandler(
                true, double(x) / screenBufOpts.width, double(y) / screenBufOpts.height,
                true, deltaXAbs, deltaYAbs
            );
        }
    }

}

void Server::keyboardEventHandler(rfbBool down, rfbKeySym keySym, rfbClientPtr cl) {
    auto& handler = options.eventHandlers.keyboard.key;
    if (handler) {
        handler(!!down, keySym);
    }
    
}

}
