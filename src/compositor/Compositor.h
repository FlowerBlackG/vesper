// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland 合成器
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#pragma once

#include "../utils/wlroots-cpp.h"
#include "./Output.h"

namespace vesper::compositor {
    
class View;

class Compositor {

public:
    Compositor();
    ~Compositor();
    int run();
    int clear();

public:
    void newOutputEventHandler(wlr_output* newOutput);
    
    
    void processCursorMotion(uint32_t timeMsec);
    
    void processCursorMove(uint32_t timeMsec);
    void processCursorResize(uint32_t timeMsec);

    View* desktopViewAt(
        double lx, double ly, wlr_surface** surface, 
        double* sx, double* sy
    );

public:

    enum class CursorMode {
        PASSTHROUGH,
        MOVE,
        RESIZE
    };

    bool initialized = false;

    wl_display* wlDisplay = nullptr;
    wlr_backend* wlrBackend = nullptr;
    wlr_renderer* wlrRenderer = nullptr;
    wlr_allocator* wlrAllocator = nullptr;
    wlr_output_layout* wlrOutputLayout = nullptr;

    wlr_scene* wlrScene = nullptr;
    wlr_scene_output_layout* wlrSceneLayout = nullptr;

    wl_list wlOutputs;

    wl_list views;
    wlr_xdg_shell* wlrXdgShell = nullptr;

    wlr_cursor* wlrCursor = nullptr;
    wlr_xcursor_manager* wlrXCursorMgr = nullptr;
    CursorMode cursorMode = CursorMode::PASSTHROUGH;

    View* grabbedView = nullptr;
    double grabX;
    double grabY;
    wlr_box grabGeoBox;

    uint32_t resizeEdges = 0;

    wl_list wlKeyboards;
    wlr_seat* wlrSeat = nullptr;

public:
    struct {
        wl_listener newOutput;
        
        wl_listener newXdgToplevel;
        wl_listener newXdgPopup;

        wl_listener cursorMotion;
        wl_listener cursorMotionAbsolute;
        wl_listener cursorButton;
        wl_listener cursorAxis;
        wl_listener cursorFrame;

        wl_listener newInput;
        wl_listener requestSetCursor;
        wl_listener requestSetSelection;
    } eventListeners;


};

}
