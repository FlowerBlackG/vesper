// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Output
 * 
 * 创建于 2023年12月27日 上海市嘉定区安亭镇
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "../../utils/ObjUtils.h"

namespace vesper::desktop::scene { class Output; }

namespace vesper::desktop::server {

class Server;

/**
 * 输出界面。
 * 每一个“屏幕”都是一个 Output。
 * 所谓“屏幕”，可能是 Linux DRM，可能是一个窗口，也可能是虚拟屏幕（离屏渲染）。
 */
class Output {

public:

    struct CreateOptions {
        Server* server;
        wlr_output* wlrOutput;
    };

    static Output* create(const CreateOptions&);

    void frameEventHandler();

protected:
    Output() {};
    int init(const CreateOptions&);
    VESPER_OBJ_UTILS_DISABLE_COPY(Output);


public:
    wl_list link;
    Server* server;
    wlr_output* wlrOutput;
    vesper::desktop::scene::Output* sceneOutput;

    struct {
        wl_listener frame; // 输出一帧
        wl_listener requestState;
        wl_listener destroy;
    } eventListeners;

};


}
