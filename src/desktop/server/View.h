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
#include <map>

namespace vesper::desktop::scene { class SceneTreeNode; }
namespace vesper::desktop::scene { class SceneRectNode; }

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
    ~View();

    void focus(wlr_surface* surface);
    void beginInteraction(Cursor::Mode cursorMode, uint32_t edges);
    bool tryUnmaximize(
        int cursorPosX = -1, 
        int cursorPosY = -1, 
        int* viewPosX = nullptr, 
        int* viewPosY = nullptr
    );


    void maximize();

    /**
     * if this view is eager to maximize (maximizeHint is not -1), then let it maximize.
     */
    bool maximizeIfEager();

    void splitScreen(bool splitLeft);
    bool splitScreenIfEager();


    bool tryMaximizeHint(int cursorPosX, int cursorPosY);
    bool trySplitHorizontallyHint(int cursorPosX, int cursorPosY);

    void xdgToplevelrequestMaximizeEventHandler();


    inline bool maximized() {
        return this->wlrXdgToplevel->current.maximized;
    }

    /**
     * 
     * 
     * 
     * @return maskID or -1 for failure.
     */
    int addColorMask(
        int x, 
        int y, 
        int width, 
        int height, 
        int32_t color,
        float opacity,
        bool absoluteToOutput
    );

    int addColorMask(
        int x, 
        int y, 
        int width, 
        int height, 
        const float color[4], 
        bool absoluteToOutput
    );

    bool removeColorMask(int id);

protected:
    View() {}
    VESPER_OBJ_UTILS_DISABLE_COPY(View);
    int init(const CreateOptions&);

public:
    wl_list link;
    Server* server;
    wlr_xdg_toplevel* wlrXdgToplevel;

    vesper::desktop::scene::SceneTreeNode* sceneTree;

    int colorMaskNextId = 1;
    std::map<int, vesper::desktop::scene::SceneRectNode*> colorMasks;
    int maximizeHintMask = -1;
    int splitLeftHintMask = -1;
    int splitRightHintMask = -1;

    bool inSplitScreenMode = false;


    struct {
        wl_listener map;
        wl_listener unmap;
        
        wl_listener toplevelDestroy;

        wl_listener commit;
        wl_listener requestMove;
        wl_listener requestResize;
        wl_listener requestMaximize;
        wl_listener requestFullscreen;

    } eventListeners;

protected:
    struct {
        int width;
        int height;
        int x;
        int y;
    } sizeBeforeMaximization = {0};


};

}

