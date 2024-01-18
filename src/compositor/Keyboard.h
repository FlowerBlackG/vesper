// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 键盘
 * 
 * 创建于 2024年1月17日 广东省珠海市
 */

#pragma once

#include "../utils/wlroots-cpp.h"

namespace vesper::compositor {

class Compositor;

class Keyboard {

public:
    int init(Compositor* compositor, wlr_keyboard* wlrKeyboard, wlr_input_device* device);


public:
    wl_list link;
    Compositor* compositor;
    wlr_keyboard* wlrKeyboard;

    struct {
        wl_listener modifiers;
        wl_listener key;
        wl_listener destroy;
    } eventListeners;
};

}