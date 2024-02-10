// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland Server
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */


#include "./Server.h"
#include "./Keyboard.h"
#include "./Cursor.h"

#include "../../log/Log.h"
#include "./View.h"
#include "../scene/Scene.h"
#include "../scene/Output.h"
#include "../scene/SceneNode.h"
#include "../scene/XdgShell.h"
#include "../scene/Surface.h"

#include <unistd.h>

using namespace std;
using namespace vesper::desktop;

namespace vesper::desktop::server {

static void newOutputEventBridge(wl_listener* listener, void* data) {
    Server* server = wl_container_of(listener, server, eventListeners.newOutput);

    server->newOutputEventHandler((wlr_output*) data);
}

/**
 * 创建新窗口时触发。
 * 
 * @param listener 
 * @param data 
 */
static void newXdgToplevelEventBridge(wl_listener* listener, void* data) {

    Server* server = wl_container_of(listener, server, eventListeners.newXdgToplevel);

    auto* toplevel = (wlr_xdg_toplevel*) data;

    View* view = View::create({
        .server = server,
        .xdgToplevel = toplevel
    });
    
    if (!view) {
        LOG_ERROR("failed to allocate View object!");
        return;
    }

}

/**
 * 创建新“气泡”时触发。
 * 气泡例：打开的一个浮动菜单。
 * 
 * @param listener 
 * @param data 
 */
static void newXdgPopupEventBridge(wl_listener* listener, void* data) {

    Server* server = wl_container_of(listener, server, eventListeners.newXdgPopup);

    auto* xdgPopup = (wlr_xdg_popup*) data;

    wlr_xdg_surface* parent = wlr_xdg_surface_try_from_wlr_surface(xdgPopup->parent);

    if (!parent) {
        LOG_ERROR("no parent for popup!");
        return;
    }

    auto* parentTree = (scene::SceneTreeNode*) parent->data;

    auto* sceneXdgSurface = scene::XdgSurface::create({
        .parent = parentTree,
        .wlrXdgSurface = xdgPopup->base
    });
    
    if (sceneXdgSurface == nullptr) {
        LOG_ERROR("failed to create xdg surface!");
        return;
    }

    xdgPopup->base->data = sceneXdgSurface->tree;
}


static void newInputEventBridge(wl_listener* listener, void* data) {
    Server* server = wl_container_of(listener, server, eventListeners.newInput);

    auto* device = (wlr_input_device*) data;
    if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
        LOG_INFO("new input detected: keyboard");
        
        auto* wlrKeyboard = wlr_keyboard_from_input_device(device);
        auto* keyboard = Keyboard::create({
            .server = server,
            .wlrKeyboard = wlrKeyboard,
            .device = device
        });

        if (!keyboard) {
            LOG_ERROR("failed to create keyboard!");
            return;
        }

    } else if (device->type == WLR_INPUT_DEVICE_POINTER) {
        LOG_INFO("new input detected: pointer");
        wlr_cursor_attach_input_device(server->cursor->wlrCursor, device);
    } else {
        LOG_INFO("input device ignored: ", device->type);
    }
    
    uint32_t capability = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards)) {
        capability |= WL_SEAT_CAPABILITY_KEYBOARD;
    }

    wlr_seat_set_capabilities(server->wlrSeat, capability);
}

/**
 * 当客户端希望设置鼠标样式时触发。
 * 
 * @param listener 
 * @param data 
 */
static void requestSetCursorEventBridge(wl_listener* listener, void* data) {
    Server* server = wl_container_of(listener, server, eventListeners.requestSetCursor);

    auto* event = (wlr_seat_pointer_request_set_cursor_event*) data;
    auto* focusedClient = server->wlrSeat->pointer_state.focused_client;

    if (focusedClient == event->seat_client) {
        wlr_cursor_set_surface(
            server->cursor->wlrCursor, event->surface,
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
    Server* server = wl_container_of(listener, server, eventListeners.requestSetSelection);

    // todo
}

Server::Server() {

}

Server::~Server() {
    
}

int Server::run() {

    this->wlDisplay = wl_display_create();
    auto waylandEventLoop = wl_display_get_event_loop(wlDisplay);
    
    bool headless = false; // todo
    if (headless) {
        this->wlrBackend = wlr_headless_backend_create(waylandEventLoop);
    } else {
        this->wlrBackend = wlr_backend_autocreate(waylandEventLoop, nullptr);
    }

    if (!wlrBackend) {
        LOG_ERROR("failed to create wlroots backend!");
        return -1;
    }

    // todo
    this->wlrRenderer = 0 ? wlr_renderer_autocreate(wlrBackend) : wlr_pixman_renderer_create();
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

    wl_list_init(&outputs);

    eventListeners.newOutput.notify = newOutputEventBridge; 
    wl_signal_add(&wlrBackend->events.new_output, &eventListeners.newOutput);

    scene = vesper::desktop::scene::Scene::create();
    if (!scene) {
        LOG_ERROR("failed to alloc scene!");
        return -1;
    }
    
    sceneLayout = scene->attachWlrOutputLayout(wlrOutputLayout);
    if (!sceneLayout) {
        LOG_ERROR("failed to attach wlr output layout!");
        return -1;
    }

    // xdg shell
    
    wl_list_init(&views);
    wlrXdgShell = wlr_xdg_shell_create(wlDisplay, 3);
    eventListeners.newXdgToplevel.notify = newXdgToplevelEventBridge;
    wl_signal_add(&wlrXdgShell->events.new_toplevel, &eventListeners.newXdgToplevel);
    eventListeners.newXdgPopup.notify = newXdgPopupEventBridge;
    wl_signal_add(&wlrXdgShell->events.new_popup, &eventListeners.newXdgPopup);

    // cursor 

    this->cursor = Cursor::create({
        .server = this,
        .wlrOutputLayout = wlrOutputLayout,
        .name = nullptr,
        .size = 24
    });


    // wayland seat

    wl_list_init(&keyboards);
    eventListeners.newInput.notify = newInputEventBridge;
    wl_signal_add(&wlrBackend->events.new_input, &eventListeners.newInput);
    wlrSeat = wlr_seat_create(wlDisplay, "seat0");
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
        execl("/bin/sh", "/bin/sh", "-c", "konsole", nullptr);
    }

    LOG_INFO("server running...");

    wl_display_run(wlDisplay); // run blocking.

    // clean up.

    wl_display_destroy_clients(wlDisplay);
    scene->destroyTree();
    delete scene;
    scene = nullptr;
    
    delete cursor;
    cursor = nullptr;

    wl_display_destroy(wlDisplay);

    return 0;
}


void Server::newOutputEventHandler(wlr_output* newOutput) {
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

    Output* output = Output::create({
        .server = this,
        .wlrOutput = newOutput
    });

    if (!output) {
        LOG_ERROR("failed to create output!");
        return;
    }

}


View* Server::desktopViewAt(
    double lx, double ly, wlr_surface** surface, 
    double* sx, double* sy
) {
    auto* node = scene->tree->nodeAt(lx, ly, sx, sy);

    if (node == nullptr || node->type() != scene::SceneNodeType::BUFFER) {
        return nullptr;
    }

    auto* sceneBuf = (scene::SceneBufferNode*) node;
    auto* sceneSurface = scene::Surface::tryFindSurfaceFromBuffer(sceneBuf);

    if (!sceneBuf) {
        return nullptr;
    }

    *surface = sceneSurface->wlrSurface;

    auto* tree = node->parent;
    while (tree && tree->data == nullptr) {
        tree = tree->parent;
    }

    return (View*) tree->data;
}


}
