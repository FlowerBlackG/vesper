// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland Server
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "../../utils/ObjUtils.h"
#include "./Output.h"

#include <unistd.h>
#include <string>
#include <vector>
#include <semaphore>

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
 * 
 * 使用步骤：
 *   1. 创建一个简单对象：
 *         Server server; 
 *       或 Server* server = new Server;
 * 
 *   2. 设置参数（关键）：
 *     server.options = {...};
 * 
 *     一定要在正式运行前设置好参数，不然可能发生意想不到的事情。
 *     参数一经设定，运行过程中不要更改！
 *     单个 Server 对象单次运行结束，可以在修改参数后重新运行（不改参数也行）。
 * 
 *   3. 运行：
 *     server.run();
 * 
 *     server 将会根据参数设定的样子运行。具体请见 RunOptions 内的说明文档。
 */
class Server {

public:
    Server() {};
    ~Server();

    struct RunOptions {

        /* ------ 外部向 Server 传递 ------ */

        struct {
            bool headless;
            struct {
                bool add;
                int width;
                int height;
            } virtualOutput;
        } backend = {0};

        struct {
            bool pixman;
        } renderer = {0};

        struct {
            bool __todo; // todo
        } session = {0};

        struct {
            std::vector<std::string> apps;    
        } launch = {};

        struct {
            bool alwaysRenderEntireScreen;
            
            bool exportScreenBuffer;
            void* exportScreenBufferDest;
        } output = {0};


        /* ------ Server 向外部返回 ------ */

        /**
         * 返回信息。由 Server 返回给外部调用者。
         */
        struct {

            std::binary_semaphore serverLaunchedSignal {0};

            int code;
            std::string msg;
            std::vector<std::string> failedApps;
        } result;

    } options;

    int run();
    void clear();

public:
    void newOutputEventHandler(wlr_output* newOutput);
    
    
    View* desktopViewAt(
        double lx, double ly, wlr_surface** surface, 
        double* sx, double* sy
    );

protected:
    VESPER_OBJ_UTILS_DISABLE_COPY(Server);



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
