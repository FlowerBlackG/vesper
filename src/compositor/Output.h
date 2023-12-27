// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Output
 * 
 * 创建于 2023年12月27日 上海市嘉定区安亭镇
 */

#pragma once

#include "../utils/wlroots-cpp.h"

namespace vesper {

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
/*
    struct {
        wl_listener map;
        wl_listener unmap;
        wl_listener destroy;
        wl_listener requestMove;
        wl_listener requestResize;
        wl_listener requestMaximize;
        wl_listener requestFullscreen;

    } eventListeners;

    wlr_scene_tree* wlrScene = nullptr;
*/
};


}