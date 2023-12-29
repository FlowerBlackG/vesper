// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Output
 * 
 * 创建于 2023年12月27日 上海市嘉定区安亭镇
 */

#pragma once

#include "../utils/wlroots-cpp.h"

namespace vesper::compositor {

class Compositor;
class Output {

public:

    int init(Compositor* compositor, wlr_output* output);

    void frameEventHandler();
    void requestStateEventHandler();
    void destroyEventHandler();

protected:


public:
    wl_list link;
    Compositor* compositor;
    wlr_output* wlrOutput;

    struct {
        wl_listener frame; // 输出一帧
        wl_listener requestState;
        wl_listener destroy;
    } eventListeners;

};


}