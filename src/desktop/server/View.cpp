// SPDX-License-Identifier: MulanPSL-2.0

/*
 * View
 * 
 * 创建于 2023年12月29日 上海市嘉定区安亭镇
 */

#include "./View.h"
#include "../../log/Log.h"
#include "./Server.h"
#include "../scene/SceneNode.h"
#include "../scene/Scene.h"
#include "../scene/XdgShell.h"

using namespace std;
using namespace vesper::desktop;

namespace vesper::desktop::server {

/**
 * called when the surface is mapped, or ready to display on-screen. 
 */
static void xdgToplevelMapEventBridge(wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, eventListeners.map);
   
    wl_list_insert(&view->server->views, &view->link);

    LOG_INFO("new xdg toplevel mapped.");

    view->focus(view->wlrXdgToplevel->base->surface);
}

/**
 * called when the surface is unmapped, and should no longer be shown.
 */
static void xdgToplevelUnmapEventBridge(wl_listener* listener, void* data) {

    View* view = wl_container_of(listener, view, eventListeners.unmap);
   
    if (view == view->server->cursor->grabbedView) {
        view->server->cursor->grabbedView = nullptr;

        view->server->cursor->cursorMode = Cursor::Mode::PASSTHROUGH;
        view->server->cursor->grabbedView = nullptr;

    }

    wl_list_remove(&view->link);
}

static void xdgToplevelDestroyEventBridge(wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, eventListeners.destroy);

    wl_list_remove(&view->eventListeners.map.link);
    wl_list_remove(&view->eventListeners.unmap.link);
    wl_list_remove(&view->eventListeners.destroy.link);
    wl_list_remove(&view->eventListeners.requestMove.link);
    wl_list_remove(&view->eventListeners.requestResize.link);
    wl_list_remove(&view->eventListeners.requestMaximize.link);
    wl_list_remove(&view->eventListeners.requestFullscreen.link);

    delete view;
}

static void xdgToplevelRequestMoveEventBridge(wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, eventListeners.requestMove);
    view->beginInteraction(Cursor::Mode::MOVE, 0);
}

/**
 * 当某个客户端希望 resize 时触发。
 * 
 * 客户端 resize 的现象：
 *   用户将鼠标移动到客户端边框，按下鼠标左键，拖动。这时，窗口跟着用户鼠标改变大小。
 * 
 * 客户端 resize 的内部过程：
 *   1. 鼠标移动到边框后，按下左键，客户端会主动发出一个 resize 请求，由本函数接收。
 *      本函数会告知 cursor 将鼠标的状态设为“resize”。
 *   2. 之后，鼠标继续移动，不断产生 motion 请求，由 cursor 处理。
 *      cursor 发现鼠标处于 resize 状态，于是修改当前聚焦窗口的尺寸。
 *   3. 鼠标左键松开后，产生 button event，由 cursor 处理。
 *      cursor 发现 event 的内容是“按键释放”，于是取消鼠标的“resize”状态标记。
 * 
 * @param listener 
 * @param data 
 */
static void xdgToplevelRequestResizeEventBridge(wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, eventListeners.requestResize);
    auto* event = (wlr_xdg_toplevel_resize_event*) data;
    view->beginInteraction(Cursor::Mode::RESIZE, event->edges);
}

static void xdgToplevelrequestMaximizeEventBridge(wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, eventListeners.requestMaximize);
    
    if (view->wlrXdgToplevel->base->initialized) {
        
        // todo: 假设仅有 1 个 output

        auto outputNode = view->server->outputs.next;
        Output* output = wl_container_of(outputNode, output, link);

        int targetHeight = output->wlrOutput->height;
        int targetWidth = output->wlrOutput->width;

        view->sceneTree->setPosition(0, 0);
        wlr_xdg_toplevel_set_size(view->wlrXdgToplevel, targetWidth, targetHeight);

        // todo
    }
}

static void xdgToplevelRequestFullscreenEventBridge(wl_listener* listener, void* data) {

    View* view = wl_container_of(listener, view, eventListeners.requestFullscreen);
    
    if (view->wlrXdgToplevel->base->initialized) {
        // todo
    }
}

VESPER_OBJ_UTILS_IMPL_CREATE(View, View::CreateOptions)


int View::init(const CreateOptions& options) {

    this->server = options.server;
    this->wlrXdgToplevel = options.xdgToplevel;


    auto* xdgSurface = scene::XdgSurface::create({
        .parent = server->scene->tree,
        .wlrXdgSurface = wlrXdgToplevel->base
    });

    if (xdgSurface == nullptr) {
        LOG_ERROR("failed to create xdg surface!");
        return -1;
    }

    this->sceneTree = xdgSurface->tree;
    
    this->sceneTree->data = this;
    wlrXdgToplevel->base->data = this->sceneTree;

    auto* wlrXdgSurface = wlrXdgToplevel->base;

    eventListeners.map.notify = xdgToplevelMapEventBridge;
    wl_signal_add(&wlrXdgSurface->surface->events.map, &eventListeners.map);

    eventListeners.unmap.notify = xdgToplevelUnmapEventBridge;
    wl_signal_add(&wlrXdgSurface->surface->events.unmap, &eventListeners.unmap);

    eventListeners.destroy.notify = xdgToplevelDestroyEventBridge;
    wl_signal_add(&wlrXdgSurface->surface->events.destroy, &eventListeners.destroy);

    auto* topLevel = wlrXdgSurface->toplevel;

    eventListeners.requestMove.notify = xdgToplevelRequestMoveEventBridge;
    wl_signal_add(&topLevel->events.request_move, &eventListeners.requestMove);

    eventListeners.requestResize.notify = xdgToplevelRequestResizeEventBridge;
    wl_signal_add(&topLevel->events.request_resize, &eventListeners.requestResize);

    eventListeners.requestMaximize.notify = xdgToplevelrequestMaximizeEventBridge;
    wl_signal_add(&topLevel->events.request_maximize, &eventListeners.requestMaximize);

    eventListeners.requestFullscreen.notify = xdgToplevelRequestFullscreenEventBridge;
    wl_signal_add(&topLevel->events.request_fullscreen, &eventListeners.requestFullscreen);

    return 0;
}

void View::focus(wlr_surface* surface) {
    auto* seat = this->server->wlrSeat;
    auto* prevSurface = seat->keyboard_state.focused_surface;
    if (prevSurface == surface) {
        return;
    }

    if (prevSurface) {
        // 取消对前一个已聚焦对象的聚焦
        auto* prevTopLevel = wlr_xdg_toplevel_try_from_wlr_surface(prevSurface);
        if (prevTopLevel) {
            wlr_xdg_toplevel_set_activated(prevTopLevel, false);
        }
    }

    auto* keyboard = wlr_seat_get_keyboard(seat);

    this->sceneTree->raiseToTop();
    wl_list_remove(&this->link);
    wl_list_insert(&server->views, &this->link);

    wlr_xdg_toplevel_set_activated(this->wlrXdgToplevel, true);

    if (keyboard) {
        wlr_seat_keyboard_notify_enter(
            seat, wlrXdgToplevel->base->surface, keyboard->keycodes, 
            keyboard->num_keycodes, &keyboard->modifiers
        );
    }

}

void View::beginInteraction(Cursor::Mode cursorMode, uint32_t edges) { // todo : move this func to Cursor class.
    auto* focusedSurface = server->wlrSeat->pointer_state.focused_surface;
    if (wlrXdgToplevel->base->surface != wlr_surface_get_root_surface(focusedSurface)) {
        return;
    }

    server->cursor->grabbedView = this;
    server->cursor->cursorMode = cursorMode;

    if (cursorMode == Cursor::Mode::MOVE) {
        server->cursor->grabX = server->cursor->wlrCursor->x - sceneTree->offset.x;
        server->cursor->grabY = server->cursor->wlrCursor->y - sceneTree->offset.y;
    } else { // resize
        
        wlr_box geoBox;
        wlr_xdg_surface_get_geometry(wlrXdgToplevel->base, &geoBox);

        double borderX = sceneTree->offset.x + geoBox.x;
        borderX += ((edges & WLR_EDGE_RIGHT) ? geoBox.width : 0);

        double borderY = sceneTree->offset.y + geoBox.y;
        borderY += ((edges & WLR_EDGE_BOTTOM) ? geoBox.height : 0);

        server->cursor->grabX = server->cursor->wlrCursor->x - borderX;
        server->cursor->grabY = server->cursor->wlrCursor->y - borderY;

        server->cursor->grabGeoBox = geoBox;
        server->cursor->grabGeoBox.x += sceneTree->offset.x;
        server->cursor->grabGeoBox.y += sceneTree->offset.y;

        server->cursor->resizeEdges = edges;
    }
    
}

}
