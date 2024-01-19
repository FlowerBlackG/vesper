// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland 合成器
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */


#include "Compositor.h"
#include "./Keyboard.h"

#include "../log/Log.h"
#include "./View.h"

#include <unistd.h>
using namespace std;


namespace vesper::compositor {

static void newOutputEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newOutput);

    compositor->newOutputEventHandler((wlr_output*) data);
}

static void newXdgToplevelEventBridge(wl_listener* listener, void* data) {

    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newXdgToplevel);

    auto* toplevel = (wlr_xdg_toplevel*) data;

    View* view = new (nothrow) View;
    if (!view) {
        LOG_ERROR("failed to allocate View object!");
        return;
    }

    view->init(compositor, toplevel);    

}

static void newXdgPopupEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newXdgPopup);

    auto* xdgPopup = (wlr_xdg_popup*) data;

    auto* parent = wlr_xdg_surface_try_from_wlr_surface(xdgPopup->parent);

    if (!parent) {
        LOG_ERROR("no parent for popup!");
        return;
    }

    auto* parentTree = (wlr_scene_tree*) parent->data;
    xdgPopup->base->data = wlr_scene_xdg_surface_create(parentTree, xdgPopup->base);
}

/**
 * 当鼠标产生“相对移动”时，本函数会被调用。
 */
static void cursorMotionEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.cursorMotion);

    auto* event = (wlr_pointer_motion_event*) data;
    wlr_cursor_move(
        compositor->wlrCursor, 
        &event->pointer->base,
        event->delta_x, event->delta_y
    );

    compositor->processCursorMotion(event->time_msec);
}

/**
 * 当鼠标产生绝对位移时触发。例如，鼠标突然出现在某个角落。 
 */
static void cursorMotionAbsoluteEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.cursorMotionAbsolute);

    auto* event = (wlr_pointer_motion_absolute_event*) data;

    wlr_cursor_warp_absolute(
        compositor->wlrCursor,
        &event->pointer->base,
        event->x, event->y
    );

    compositor->processCursorMotion(event->time_msec);
}

/**
 * 当鼠标按键被按下时触发。 
 */
static void cursorButtonEventBridge(wl_listener* listener, void* data) {
    LOG_TEMPORARY("event!!");
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.cursorButton);

    auto* event = (wlr_pointer_button_event*) data;

    wlr_seat_pointer_notify_button(
        compositor->wlrSeat, event->time_msec, event->button, event->state
    );

    double sx, sy;
    wlr_surface* surface = nullptr;
    View* view = compositor->desktopViewAt(
        compositor->wlrCursor->x, compositor->wlrCursor->y, &surface, &sx, &sy
    );

    if (event->state == WLR_BUTTON_RELEASED) {
        compositor->cursorMode = Compositor::CursorMode::PASSTHROUGH;
        compositor->grabbedView = nullptr;
    } else if (event->state == WLR_BUTTON_PRESSED) {
        if (view) {
            view->focus(surface);
        }
    }

}

/**
 * 由坐标轴事件触发。也就是说...比如滚动鼠标滚轮的时候。 
 */
static void cursorAxisEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.cursorAxis);

    auto* event = (wlr_pointer_axis_event*) data;

    wlr_seat_pointer_notify_axis(
        compositor->wlrSeat, event->time_msec, event->orientation,
        event->delta, event->delta_discrete, event->source
    );
}

static void cursorFrameEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.cursorFrame);

    wlr_seat_pointer_notify_frame(compositor->wlrSeat);
}

static void newInputEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newInput);

    auto* device = (wlr_input_device*) data;
    if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
        LOG_INFO("new input detected: keyboard");
        
        auto* wlrKeyboard = wlr_keyboard_from_input_device(device);
        auto* keyboard = new (nothrow) Keyboard;
        if (!keyboard) {
            LOG_ERROR("failed to create keyboard!");
            return;
        }

        keyboard->init(compositor, wlrKeyboard, device);

    } else if (device->type == WLR_INPUT_DEVICE_POINTER) {
        LOG_INFO("new input detected: pointer");
        wlr_cursor_attach_input_device(compositor->wlrCursor, device);
    } else {
        LOG_INFO("input device ignored: ", device->type);
    }
    
    uint32_t capability = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&compositor->wlKeyboards)) {
        capability |= WL_SEAT_CAPABILITY_KEYBOARD;
    }

    wlr_seat_set_capabilities(compositor->wlrSeat, capability);
}

/**
 * 当客户端希望设置鼠标样式时触发。
 * 
 * @param listener 
 * @param data 
 */
static void requestSetCursorEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.requestSetCursor);

    auto* event = (wlr_seat_pointer_request_set_cursor_event*) data;
    auto* focusedClient = compositor->wlrSeat->pointer_state.focused_client;

    if (focusedClient == event->seat_client) {
        wlr_cursor_set_surface(
            compositor->wlrCursor, event->surface,
            event->hotspot_x, event->hotspot_y
        );
    }
}

/**
 * 当用户产生“拷贝”时触发。
 * 
 * @param listener 
 * @param data 
 */
static void requestSetSelectionEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.requestSetSelection);

    // todo
}

Compositor::Compositor() {

}

Compositor::~Compositor() {
    this->clear();
}

int Compositor::run() {
    if (initialized) {
        return 0;
    }

    this->wlDisplay = wl_display_create();
    this->wlrBackend = wlr_backend_autocreate(wlDisplay, nullptr);
    if (!wlrBackend) {
        LOG_ERROR("failed to create wlroots backend!");
        return -1;
    }

    this->wlrRenderer = wlr_renderer_autocreate(wlrBackend);
    if (!wlrRenderer) {
        LOG_ERROR("failed to create wlroots renderer!");
        return -1;
    }

    wlr_renderer_init_wl_display(wlrRenderer, wlDisplay);

    wlrAllocator = wlr_allocator_autocreate(wlrBackend, wlrRenderer);
    if (!wlrAllocator) {
        LOG_ERROR("failed to create wlroots allocator!");
        return -1;
    }

    wlr_compositor_create(wlDisplay, 5, wlrRenderer);
    wlr_subcompositor_create(wlDisplay);
    wlr_data_device_manager_create(wlDisplay);

    wlrOutputLayout = wlr_output_layout_create(wlDisplay);

    wl_list_init(&wlOutputs);

    eventListeners.newOutput.notify = newOutputEventBridge; 
    wl_signal_add(&wlrBackend->events.new_output, &eventListeners.newOutput);

    wlrScene = wlr_scene_create();
    wlrSceneLayout = wlr_scene_attach_output_layout(wlrScene, wlrOutputLayout);

    // xdg shell
    wl_list_init(&views);
    wlrXdgShell = wlr_xdg_shell_create(wlDisplay, 3);
    eventListeners.newXdgToplevel.notify = newXdgToplevelEventBridge;
    wl_signal_add(&wlrXdgShell->events.new_toplevel, &eventListeners.newXdgToplevel);
    eventListeners.newXdgPopup.notify = newXdgPopupEventBridge;
    wl_signal_add(&wlrXdgShell->events.new_popup, &eventListeners.newXdgPopup);

    // wlroots cursor 
    wlrCursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(wlrCursor, wlrOutputLayout);

    // wlr xcursor manager
    // it loads up xcursor themes to source cursor images from and 
    // make sure that cursor images are available at all scale factors
    // on screen.
    wlrXCursorMgr = wlr_xcursor_manager_create(nullptr, 24);

    // cursor
    cursorMode = CursorMode::PASSTHROUGH;
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

    // wayland seat
    wl_list_init(&wlKeyboards);
    eventListeners.newInput.notify = newInputEventBridge;
    wl_signal_add(&wlrBackend->events.new_input, &eventListeners.newInput);
    wlrSeat = wlr_seat_create(wlDisplay, "seat0"); // todo: 真的是 seat0 么？
    eventListeners.requestSetCursor.notify = requestSetCursorEventBridge;
    wl_signal_add(&wlrSeat->events.request_set_cursor, &eventListeners.requestSetCursor);
    eventListeners.requestSetSelection.notify = requestSetSelectionEventBridge;
    wl_signal_add(&wlrSeat->events.request_set_selection, &eventListeners.requestSetSelection);


    // socket for Wayland display.

    const char* socket = wl_display_add_socket_auto(wlDisplay);
    if (!socket) {
        LOG_ERROR("failed to create wayland display socket!");
        wlr_backend_destroy(wlrBackend);
        return -1;
    }

    if (!wlr_backend_start(wlrBackend)) {
        wlr_backend_destroy(wlrBackend);
        wl_display_destroy(wlDisplay);
        LOG_ERROR("failed to start wlr backend!");
        return -1;
    }

    LOG_INFO("running wayland display with socket: ", socket);

    setenv("WAYLAND_DISPLAY", socket, true); 
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", "konsole", 0);
    }

    LOG_INFO("compositor running...");

    wl_display_run(wlDisplay); // run blocking.

    // clean up.

    wl_display_destroy_clients(wlDisplay);
    wlr_scene_node_destroy(&wlrScene->tree.node);
    wlr_xcursor_manager_destroy(wlrXCursorMgr);

    wl_display_destroy(wlDisplay);

    return 0;
}


int Compositor::clear() {
    return 0;
}


void Compositor::newOutputEventHandler(wlr_output* newOutput) {
    wlr_output_init_render(newOutput, wlrAllocator, wlrRenderer);

    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    
    wlr_output_mode* mode = wlr_output_preferred_mode(newOutput);

    if (mode) {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_commit_state(newOutput, &state);
    wlr_output_state_finish(&state);

    Output* output = new (nothrow) Output;
    if (!output) {
        LOG_ERROR("failed to create output!");
        return;
    }

    output->init(this, newOutput);


}

void Compositor::processCursorMotion(uint32_t timeMsec) {

    if (cursorMode == CursorMode::MOVE) {
        processCursorMove(timeMsec);
        return;
    } else if (cursorMode == CursorMode::RESIZE) {
        LOG_TEMPORARY("cursor resize");
        processCursorResize(timeMsec);
        return;
    }

    double sx, sy;
    wlr_surface* surface = nullptr;
    auto* topLevel = this->desktopViewAt(
        wlrCursor->x, wlrCursor->y, &surface, &sx, &sy
    );

    if (!topLevel) {
        // 如果鼠标不在某个 View 上，就显示默认图像。
        wlr_cursor_set_xcursor(wlrCursor, wlrXCursorMgr, "default");
    }

    if (surface) {
        wlr_seat_pointer_notify_enter(wlrSeat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(wlrSeat, timeMsec, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(wlrSeat);
    }

}


void Compositor::processCursorMove(uint32_t timeMsec) {
    auto* topLevel = grabbedView;
    wlr_scene_node_set_position(
        &topLevel->wlrSceneTree->node, 
        wlrCursor->x - grabX, wlrCursor->y - grabY
    );
}

void Compositor::processCursorResize(uint32_t timeMsec) {
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
    wlr_scene_node_set_position(
        &view->wlrSceneTree->node,
        newLeft - geoBox.x,
        newTop - geoBox.y 
    );

    int newWidth = newRight - newLeft;
    int newHeight = newBottom - newTop;

    wlr_xdg_toplevel_set_size(
        view->wlrXdgToplevel, newWidth, newHeight
    );
}

View* Compositor::desktopViewAt(
    double lx, double ly, wlr_surface** surface, 
    double* sx, double* sy
) {
    auto* node = wlr_scene_node_at(&wlrScene->tree.node, lx, ly, sx, sy);

    if (node == nullptr || node->type != WLR_SCENE_NODE_BUFFER) {
        return nullptr;
    }

    auto* sceneBuf = wlr_scene_buffer_from_node(node);
    auto* sceneSurface = wlr_scene_surface_try_from_buffer(sceneBuf);

    if (!sceneBuf) {
        return nullptr;
    }

    *surface = sceneSurface->surface;

    auto* tree = node->parent;
    while (tree && tree->node.data == nullptr) {
        tree = tree->node.parent;
    }

    return (View*) tree->node.data;
}


}
