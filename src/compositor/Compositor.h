// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland 合成器
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#pragma once

#include "../utils/wlroots-cpp.h"
#include "./Output.h"

namespace vesper {
    
class Compositor {

public:
    Compositor();
    ~Compositor();
    int init();
    int run();
    int clear();

public:
    void newOutputEventHandler(wlr_output* newOutput);
    void newXdgSurfaceEventHandler(wlr_xdg_surface* newXdgSurface);

public:

    bool initialized = false;

    wl_display* wlDisplay = nullptr;
    wlr_backend* wlrBackend = nullptr;
    wlr_renderer* wlrRenderer = nullptr;
    wlr_allocator* wlrAllocator = nullptr;
    wlr_output_layout* wlrOutputLayout = nullptr;

    wlr_scene* wlrScene = nullptr;
    wlr_scene_output_layout* wlrSceneLayout = nullptr;

    wl_list wlOutputs;

    wl_list wlViews;
    wlr_xdg_shell* wlrXdgShell = nullptr;

public:
    struct {
        wl_listener newOutput;
        wl_listener newXdgSurface;
    } eventListeners;


};

}
