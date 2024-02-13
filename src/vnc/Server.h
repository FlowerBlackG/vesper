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

#include <rfb/rfb.h>

namespace vesper::vnc {


class Server {

public:

    Server() {};
    ~Server();
    
    struct RunOptions {

        struct {
            int width;
            int height;
            std::function<char* ()> getBuffer;
        } screenBuffer;

        struct {
            struct {
                std::function<void (
                    bool absolute, double absoluteX, double absoluteY,
                    bool delta, int deltaX, int deltaY
                )> motion;
            } mouse;
            
        } eventHandlers;
        
        struct {

            std::binary_semaphore serverLaunchedSignal {0};

            int code;
            std::string msg;
        } result;
    } options = {0};

    int run();

    void clear();

public:


public:
    void mouseEventHandler(int buttonMask, int x, int y, rfbClientPtr cl);
    void keyboardEventHandler(rfbBool down, rfbKeySym keySym, rfbClientPtr cl);

protected:
    rfbScreenInfoPtr rfbServer = nullptr;


};


}
