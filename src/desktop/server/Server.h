// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland Server
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "./Output.h"

namespace vesper::desktop::scene { class Scene; }
namespace vesper::desktop::scene { class OutputLayout; }

namespace vesper::desktop::server {
    
class View;

/**
 * Wayland Server
 * 
 * 该模块是一个基于 wlroots 的 Wayland 图形服务器。负责完成创建 Wayland 服务器、
 * 接收和转交键鼠请求等任务。
 */
class Server {

public:
    Server();
    ~Server();
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

    vesper::desktop::scene::Scene* scene = nullptr;
    vesper::desktop::scene::OutputLayout* sceneLayout = nullptr;

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
