// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 键盘
 * 
 * 创建于 2024年1月17日 广东省珠海市
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "../../utils/ObjUtils.h"

namespace vesper::desktop::server {

class Server;

class Keyboard {

public:

    struct CreateOptions {
        Server* server;
        wlr_keyboard* wlrKeyboard;
        wlr_input_device* device;
    };

    static Keyboard* create(const CreateOptions& options);

protected:
    Keyboard() {};

    VESPER_OBJ_UTILS_DISABLE_COPY(Keyboard);

    int init(const CreateOptions& options);


public:
    wl_list link;
    Server* server;
    wlr_keyboard* wlrKeyboard;

    struct {
        wl_listener modifiers;
        wl_listener key;
        wl_listener destroy;
    } eventListeners;
};

}