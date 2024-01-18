// SPDX-License-Identifier: MulanPSL-2.0

/*
 * View
 * 
 * 创建于 2023年12月29日 上海市嘉定区安亭镇
 */

#pragma once

#include "../utils/wlroots-cpp.h"


namespace vesper::compositor {

class Compositor;

class View {

public:
    int init(Compositor* compositor, wlr_xdg_toplevel* xdgToplevel);

    void focus(wlr_surface* surface);

public:
    wl_list link;
    Compositor* compositor;
    wlr_xdg_toplevel* wlrXdgToplevel;
    wlr_scene_tree* wlrSceneTree;


    struct {
        wl_listener map;
        wl_listener unmap;
        wl_listener destroy;
        wl_listener requestMove;
        wl_listener requestResize;
        wl_listener requestMaximize;
        wl_listener requestFullscreen;

    } eventListeners;


};

}

