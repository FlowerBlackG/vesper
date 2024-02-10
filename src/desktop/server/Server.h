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
class Cursor;

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

public:
    void newOutputEventHandler(wlr_output* newOutput);
    
    
    View* desktopViewAt(
        double lx, double ly, wlr_surface** surface, 
        double* sx, double* sy
    );

public:

    

    wl_display* wlDisplay = nullptr;
    wlr_backend* wlrBackend = nullptr;
    wlr_renderer* wlrRenderer = nullptr;
    wlr_allocator* wlrAllocator = nullptr;
    wlr_output_layout* wlrOutputLayout = nullptr;

    vesper::desktop::scene::Scene* scene = nullptr;
    vesper::desktop::scene::OutputLayout* sceneLayout = nullptr;

    /**
     * 
     * 成员类型：scene::server::Output
     */
    wl_list outputs;

    wl_list views;
    wlr_xdg_shell* wlrXdgShell = nullptr;

    Cursor* cursor = nullptr;

    /**
     * 
     * 成员类型：scene::server::Keyboard
     */
    wl_list keyboards;
    wlr_seat* wlrSeat = nullptr;

public:
    struct {
        wl_listener newOutput;
        
        wl_listener newXdgToplevel;
        wl_listener newXdgPopup;

        wl_listener newInput;
        wl_listener requestSetCursor;
        wl_listener requestSetSelection;
    } eventListeners;


};

}
