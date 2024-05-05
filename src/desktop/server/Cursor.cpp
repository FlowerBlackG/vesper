// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 鼠标
 * 
 * 创建于 2024年2月10日 北京市海淀区
 */

#include "./Cursor.h"
#include "./Server.h"
#include "./View.h"

#include "../scene/Scene.h"
#include "../scene/SceneNode.h"
#include "../scene/Output.h"

using namespace std;
using namespace vesper::bindings;

namespace vesper::desktop::server {


/**
 * 当鼠标产生“相对移动”时，本函数会被调用。
 */
static void cursorMotionEventBridge(wl_listener* listener, void* data) {
    Cursor* cursor = wl_container_of(listener, cursor, eventListeners.cursorMotion);

    auto* event = (wlr_pointer_motion_event*) data;
    wlr_cursor_move(
        cursor->wlrCursor, 
        &event->pointer->base,
        event->delta_x, event->delta_y
    );

    cursor->processMotion(event->time_msec);
}

/**
 * 当鼠标产生绝对位移时触发。例如，鼠标突然出现在某个角落。 
 */
static void cursorMotionAbsoluteEventBridge(wl_listener* listener, void* data) {
    Cursor* cursor = wl_container_of(listener, cursor, eventListeners.cursorMotionAbsolute);

    auto* event = (wlr_pointer_motion_absolute_event*) data;

    wlr_cursor_warp_absolute(
        cursor->wlrCursor,
        &event->pointer->base,
        event->x, event->y
    );

    cursor->processMotion(event->time_msec);
}

/**
 * 当鼠标按键被按下时触发。 
 */
static void cursorButtonEventBridge(wl_listener* listener, void* data) {
    
    Cursor* cursor = wl_container_of(listener, cursor, eventListeners.cursorButton);

    auto* event = (wlr_pointer_button_event*) data;
    cursor->buttonEventHandler(event->time_msec, event->button, event->state);

}

/**
 * 由坐标轴事件触发。也就是说...比如滚动鼠标滚轮的时候。 
 */
static void cursorAxisEventBridge(wl_listener* listener, void* data) {
    Cursor* cursor = wl_container_of(listener, cursor, eventListeners.cursorAxis);

    auto* event = (wlr_pointer_axis_event*) data;

    wlr_seat_pointer_notify_axis(
        cursor->server->wlrSeat, event->time_msec, event->orientation,
        event->delta, event->delta_discrete, event->source,
        event->relative_direction
    );
}

static void cursorFrameEventBridge(wl_listener* listener, void* data) {
    Cursor* cursor = wl_container_of(listener, cursor, eventListeners.cursorFrame);

    wlr_seat_pointer_notify_frame(cursor->server->wlrSeat);
}



VESPER_OBJ_UTILS_IMPL_CREATE(Cursor, Cursor::CreateOptions)

int Cursor::init(const CreateOptions& options) {
    this->server = options.server;
    this->wlrCursor = wlr_cursor_create();

    wlr_cursor_attach_output_layout(wlrCursor, options.wlrOutputLayout);

    /*
        wlr xcursor manager
        it loads up xcursor themes to source cursor images from and 
        make sure that cursor images are available at all scale factors
        on screen.
    */
    wlrXCursorMgr = wlr_xcursor_manager_create(options.name, options.size);

    eventListeners.cursorMotion.notify = cursorMotionEventBridge;
    wl_signal_add(&wlrCursor->events.motion, &eventListeners.cursorMotion);
    eventListeners.cursorMotionAbsolute.notify = cursorMotionAbsoluteEventBridge;
    wl_signal_add(&wlrCursor->events.motion_absolute, &eventListeners.cursorMotionAbsolute);
    eventListeners.cursorButton.notify = cursorButtonEventBridge;
    wl_signal_add(&wlrCursor->events.button, &eventListeners.cursorButton);
    eventListeners.cursorAxis.notify = cursorAxisEventBridge;
    wl_signal_add(&wlrCursor->events.axis, &eventListeners.cursorAxis);
    eventListeners.cursorFrame.notify = cursorFrameEventBridge;
    wl_signal_add(&wlrCursor->events.frame, &eventListeners.cursorFrame);

    return 0;
}

Cursor::~Cursor() {
    wlr_xcursor_manager_destroy(wlrXCursorMgr);
    wlr_cursor_destroy(wlrCursor);
}


void Cursor::processMotion(uint32_t timeMsec) {
    
    if (cursorMode == Cursor::Mode::MOVE) {
        processMove(timeMsec);
        return;
    } else if (cursorMode == Cursor::Mode::RESIZE) {
        processResize(timeMsec);
        return;
    }

    double sx, sy;
    wlr_surface* surface = nullptr;
    auto* topLevel = server->desktopViewAt(
        wlrCursor->x, wlrCursor->y, &surface, &sx, &sy
    );

    if (!topLevel) {
        // 如果鼠标不在某个 View 上，就显示默认图像。
        wlr_cursor_set_xcursor(wlrCursor, wlrXCursorMgr, "default");
    }

    if (surface) {
        wlr_seat_pointer_notify_enter(server->wlrSeat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server->wlrSeat, timeMsec, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(server->wlrSeat);
    }

}


void Cursor::processMove(uint32_t timeMsec) {
    int viewPosX, viewPosY;

    // this function is called only when grabbedView is not null.

    if (grabbedView->tryUnmaximize(wlrCursor->x, wlrCursor->y, &viewPosX, &viewPosY)) {
        grabX = wlrCursor->x - viewPosX;
        grabY = wlrCursor->y - viewPosY;
    } else {
        grabbedView->sceneTree->setPosition(wlrCursor->x - grabX, wlrCursor->y - grabY);
    }

    // maximize or split screen hint
    if (grabbedView->tryMaximizeHint(wlrCursor->x, wlrCursor->y)) {
        // do nothing.
    } else if (grabbedView->trySplitHorizontallyHint(wlrCursor->x, wlrCursor->y)) {
        // do nothing.
    }
}


void Cursor::processResize(uint32_t timeMsec) {
    View* view = grabbedView;
    double borderX = wlrCursor->x - grabX;
    double borderY = wlrCursor->y - grabY;
    int newLeft = grabGeoBox.x;
    int newRight = grabGeoBox.x + grabGeoBox.width;
    int newTop = grabGeoBox.y;
    int newBottom = grabGeoBox.y + grabGeoBox.height;

    if (resizeEdges & WLR_EDGE_TOP) {
        newTop = borderY;
        if (newTop >= newBottom) {
            newTop = newBottom - 1;
        }
    } else if (resizeEdges & WLR_EDGE_BOTTOM) {
        newBottom = borderY;
        if (newBottom <= newTop) {
            newBottom = newTop + 1;
        }
    }

    if (resizeEdges & WLR_EDGE_LEFT) {
        newLeft = borderX;
        if (newLeft >= newRight) {
            newLeft = newRight - 1;
        }
    } else if (resizeEdges & WLR_EDGE_RIGHT) {
        newRight = borderX;
        if (newRight <= newLeft) {
            newRight = newLeft + 1;
        }
    }

    wlr_box geoBox;
    wlr_xdg_surface_get_geometry(view->wlrXdgToplevel->base, &geoBox);

    view->sceneTree->setPosition(newLeft - geoBox.x, newTop - geoBox.y);

    int newWidth = newRight - newLeft;
    int newHeight = newBottom - newTop;

    wlr_xdg_toplevel_set_size(
        view->wlrXdgToplevel, newWidth, newHeight
    );
}

void Cursor::buttonEventHandler(
    uint32_t timeMsec, uint32_t button, wl_pointer_button_state state
) {

    wlr_seat_pointer_notify_button(server->wlrSeat, timeMsec, button, state);

    double sx, sy;
    wlr_surface* surface = nullptr;
    View* view = server->desktopViewAt(
        wlrCursor->x, wlrCursor->y, &surface, &sx, &sy
    );

    if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
        if (this->cursorMode != Cursor::Mode::PASSTHROUGH && grabbedView) {
            grabbedView->maximizeIfEager() || grabbedView->splitScreenIfEager();
        }

        this->cursorMode = Cursor::Mode::PASSTHROUGH;
        this->grabbedView = nullptr;
    } else if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        if (view) {
            view->focus(surface);
        }
    }
}

} // namespace vesper::desktop::server
