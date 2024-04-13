// SPDX-License-Identifier: MulanPSL-2.0

/*
 * View
 * 
 * 创建于 2023年12月29日 上海市嘉定区安亭镇
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "../../utils/ObjUtils.h"
#include "./Cursor.h"

namespace vesper::desktop::scene { class SceneTreeNode; }

namespace vesper::desktop::server {

/**
 * XDG 窗口。
 * 
 * 每个窗口都是一个 View。
 * 例如，打开 QQ 时会创建一个 View；
 * 在新窗口打开聊天框时，新窗口是独立的 View；
 * 打开“QQ设置”对话框，对话框也是一个独立的 View。
 * 
 * 合成器内的 View 对象不关心自己在屏幕上的位置等信息。那些是场景管理器要关心的。
 */
class View {

public:

    struct CreateOptions {
        Server* server;
        wlr_xdg_toplevel* xdgToplevel;
    };

    static View* create(const CreateOptions&);

    void focus(wlr_surface* surface);
    void beginInteraction(Cursor::Mode cursorMode, uint32_t edges);

protected:
    View() {}
    VESPER_OBJ_UTILS_DISABLE_COPY(View);
    int init(const CreateOptions&);

public:
    wl_list link;
    Server* server;
    wlr_xdg_toplevel* wlrXdgToplevel;

    vesper::desktop::scene::SceneTreeNode* sceneTree;


    struct {
        wl_listener map;
        wl_listener unmap;
        wl_listener destroy;
        wl_listener commit;
        wl_listener requestMove;
        wl_listener requestResize;
        wl_listener requestMaximize;
        wl_listener requestFullscreen;

    } eventListeners;


};

}

