// SPDX-License-Identifier: MulanPSL-2.0

/*
 * View
 * 
 * 创建于 2023年12月29日 上海市嘉定区安亭镇
 */

#include "./View.h"
#include "../log/Log.h"
#include "./Compositor.h"
using namespace std;

namespace vesper::compositor {

/**
 * called when the surface is mapped, or ready to display on-screen. 
 */
static void xdgToplevelMapEventBridge(wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, eventListeners.map);
    
    wl_list_insert(&view->compositor->views, &view->link);

    LOG_INFO("new xdg toplevel mapped.");
    LOG_TEMPORARY("new xdg toplevel mapped.");

    view->focus(view->wlrXdgToplevel->base->surface);
}

/**
 * called when the surface is unmapped, and should no longer be shown.
 */
static void xdgToplevelUnmapEventBridge(wl_listener* listener, void* data) {

    View* view = wl_container_of(listener, view, eventListeners.unmap);
    
    if (view == view->compositor->grabbedView) {
        view->compositor->grabbedView = nullptr;

        view->compositor->cursorMode = Compositor::CursorMode::PASSTHROUGH;
        view->compositor->grabbedView = nullptr;

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
// todo
    //view->xdgToplevelRequestMoveEventHandler();
}

static void xdgToplevelRequestResizeEventBridge(wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, eventListeners.requestResize);
    // todo
    //view->xdgToplevelRequestResizeEventHandler();
}

static void xdgToplevelrequestMaximizeEventBridge(wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, eventListeners.requestMaximize);
    // todo
    //view->xdgToplevelRequestMaximizeEventHandler();
}

static void xdgToplevelRequestFullscreenEventBridge(wl_listener* listener, void* data) {

    View* view = wl_container_of(listener, view, eventListeners.requestFullscreen);
    // todo
    //view->xdgToplevelRequestFullscreenEventHandler();
}




int View::init(Compositor* compositor, wlr_xdg_toplevel* xdgToplevel) {

    this->compositor = compositor;
    this->wlrXdgToplevel = xdgToplevel;

    wlrSceneTree = wlr_scene_xdg_surface_create(
        &this->compositor->wlrScene->tree, xdgToplevel->base
    );

    wlrSceneTree->node.data = this;
    xdgToplevel->base->data = this->wlrSceneTree;
LOG_TEMPORARY("view init")

    auto* xdgSurface = xdgToplevel->base;

    eventListeners.map.notify = xdgToplevelMapEventBridge;
    wl_signal_add(&xdgSurface->surface->events.map, &eventListeners.map);

    eventListeners.unmap.notify = xdgToplevelUnmapEventBridge;
    wl_signal_add(&xdgSurface->surface->events.unmap, &eventListeners.unmap);

    eventListeners.destroy.notify = xdgToplevelDestroyEventBridge;
    wl_signal_add(&xdgSurface->surface->events.destroy, &eventListeners.destroy);

    auto* topLevel = xdgSurface->toplevel;

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
    auto* seat = this->compositor->wlrSeat;
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

    wlr_scene_node_raise_to_top(&wlrSceneTree->node);
    wl_list_remove(&link);
    wl_list_insert(&compositor->views, &link);

    wlr_xdg_toplevel_set_activated(this->wlrXdgToplevel, true);

    if (keyboard) {
        wlr_seat_keyboard_notify_enter(
            seat, wlrXdgToplevel->base->surface, keyboard->keycodes, 
            keyboard->num_keycodes, &keyboard->modifiers
        );
    }

}

}
