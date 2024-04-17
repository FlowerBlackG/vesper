// SPDX-License-Identifier: MulanPSL-2.0

/*
 * VNC 服务
 *
 * 创建于 2024年2月13日 北京市海淀区
 */

#pragma once

#include <functional>
#include <string>
#include <semaphore>
#include "../log/Log.h"

#include "../common/MouseButton.h"

#include <rfb/rfb.h>
#include <xkbcommon/xkbcommon.h>

#include "../bindings/pixman.h"


namespace vesper::vnc {


class Server {

public:

    Server() {};
    ~Server();
    
    struct RunOptions {

        struct {
            int width;
            int height;
            std::function<void* (vesper::bindings::pixman::Region32& damage)> getBuffer;
            std::function<void (void*)> recycleBuffer;
        } screenBuffer;

        struct {
            struct {
                std::function<void (
                    bool absolute, double absoluteX, double absoluteY,
                    bool delta, int deltaX, int deltaY
                )> motion;

                std::function<void (bool press, vesper::common::MouseButton)> button;

                /**
                 * 
                 * 
                 * @param vertical true 表示垂直方向。false 表示水平方向。
                 * @param delta 从左到右为正；从上到下为正。反向传负数即可。
                 */
                std::function<void (bool vertical, double delta, int32_t deltaDiscrete)> axis;
 
            } mouse;

            struct {
                std::function<void (bool pressed, xkb_keysym_t keysym)> key;
            } keyboard;
            
        } eventHandlers;

        struct {
            std::string password;
            std::string libvncserverPasswdFile;
        } auth;

        struct {
            /** -1 表示不设置。 */
            int port = -1;
        } net;

        struct {

            std::binary_semaphore serverLaunchedSignal {0};

            int code;
            std::string msg;
        } result;
    } options = {0};

    int run();

    void clear();

    void terminate();

public:

    struct {
        int prevX = -1;
        int prevY = -1;
        int prevButtonMask = 0;
    } mouseData;

    void mouseEventHandler(int buttonMask, int x, int y, rfbClientPtr cl);
    void keyboardEventHandler(rfbBool down, rfbKeySym keySym, rfbClientPtr cl);

protected:
    rfbScreenInfoPtr rfbServer = nullptr;
    bool systemRunning;

    char* framebufferFallback = nullptr;
    vesper::bindings::pixman::Region32 frameDamage;

};


}
