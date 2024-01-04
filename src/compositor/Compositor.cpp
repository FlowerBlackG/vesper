// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland 合成器
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */


#include "Compositor.h"
#include "../log/Log.h"
#include "./View.h"

#include <unistd.h>
using namespace std;


namespace vesper::compositor {

static void newOutputEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newOutput);

    compositor->newOutputEventHandler((wlr_output*) data);
}

static void newXdgSurfaceEventBridge(wl_listener* listener, void* data) {

    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newXdgSurface);
    

    compositor->newXdgSurfaceEventHandler((wlr_xdg_surface*) data);
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

    // todo: process cursor motion
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

    // todo: process cursor motion
}

/**
 * 当鼠标按键被按下时触发。 
 */
static void cursorButtonEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.cursorButton);

    auto* event = (wlr_pointer_button_event*) data;
    // todo

    // todo
}

/**
 * 由坐标轴事件触发。也就是说...比如滚动鼠标滚轮的时候。 
 */
static void cursorAxisEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.cursorAxis);

    // todo
}

static void cursorFrameEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.cursorFrame);


    // todo
}

static void newInputEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newInput);


    // todo
}

static void requestSetCursorEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.requestSetCursor);


    // todo
}

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

    wlrOutputLayout = wlr_output_layout_create();

    wl_list_init(&wlOutputs);

    eventListeners.newOutput.notify = newOutputEventBridge; 
    wl_signal_add(&wlrBackend->events.new_output, &eventListeners.newOutput);

    wlrScene = wlr_scene_create();
    wlrSceneLayout = wlr_scene_attach_output_layout(wlrScene, wlrOutputLayout);

    // xdg shell
    wl_list_init(&wlViews);
    wlrXdgShell = wlr_xdg_shell_create(wlDisplay, 3);
    eventListeners.newXdgSurface.notify = newXdgSurfaceEventBridge;
    wl_signal_add(&wlrXdgShell->events.new_surface, &eventListeners.newXdgSurface);

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
        execl("/bin/sh", "/bin/sh", "-c", "kitty", 0);
    }

    LOG_INFO("compositor running...");
    wl_display_run(wlDisplay);

    return 0;
}


int Compositor::clear() {

    if (!initialized) {
        return 0;
    }

    if (false && wlDisplay) {
        wl_display_destroy_clients(wlDisplay);
        wl_display_destroy(wlDisplay);
        wlDisplay = nullptr;
    }
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
        // todo
    }

    output->init(this, newOutput);


}

void Compositor::newXdgSurfaceEventHandler(wlr_xdg_surface* newXdgSurface) {
    if (newXdgSurface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        wlr_xdg_surface* parent = wlr_xdg_surface_try_from_wlr_surface(
            newXdgSurface->popup->parent
        );

        if (parent == nullptr) {
            LOG_ERROR("no parent for new wlroots xdg surface!");
            return;
        }

        auto* parentTree = (wlr_scene_tree*) parent->data;
        newXdgSurface->data = wlr_scene_xdg_surface_create(
            parentTree, newXdgSurface
        );

        return;
    }


    if (newXdgSurface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        LOG_ERROR("bad role!");
        return;
    }

    auto* view = new (nothrow) View;
    if (!view) {
        LOG_ERROR("failed to alloc view!");
        return;
    }

    view->init(this, newXdgSurface);

}


}
