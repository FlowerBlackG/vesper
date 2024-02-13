// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland Server
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */


#include "./Server.h"
#include "./Keyboard.h"
#include "./Cursor.h"
#include "./Output.h"

#include "../../log/Log.h"
#include "./View.h"
#include "../scene/Scene.h"
#include "../scene/Output.h"
#include "../scene/SceneNode.h"
#include "../scene/XdgShell.h"
#include "../scene/Surface.h"

#include <unistd.h>
#include <thread>

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

Server::~Server() {
    
}

static void clearRunOptionsResult(Server::RunOptions& options) {
    
    auto& res = options.result;
    res.msg.clear();
    res.failedApps.clear();

    res.serverLaunchedSignal.try_acquire();
}

int Server::run() {
    
    clearRunOptionsResult(options);

    this->wlDisplay = wl_display_create();
    auto wlEventLoop = wl_display_get_event_loop(wlDisplay);
    
    // backend
    
    if (options.backend.headless) {
        this->wlrBackend = wlr_headless_backend_create(wlEventLoop);
        if (this->wlrBackend && options.backend.virtualOutput.add) {
            wlr_headless_add_output(
                wlrBackend,
                options.backend.virtualOutput.width, 
                options.backend.virtualOutput.height
            );
        }
    } else {
        this->wlrBackend = wlr_backend_autocreate(wlEventLoop, nullptr);
    }

    if (!wlrBackend) {
        LOG_ERROR("failed to create wlroots backend!");
        options.result.code = -1;
        return -1;
    }

    // renderer

    if (options.renderer.pixman) {
        this->wlrRenderer = wlr_pixman_renderer_create();
    } else {
        this->wlrRenderer = wlr_renderer_autocreate(wlrBackend);
    }
    
    if (!wlrRenderer) {
        LOG_ERROR("failed to create wlroots renderer!");
        options.result.code = -1;
        return -1;
    }

    wlr_renderer_init_wl_display(wlrRenderer, wlDisplay);

    wlrAllocator = wlr_allocator_autocreate(wlrBackend, wlrRenderer);
    if (!wlrAllocator) {
        LOG_ERROR("failed to create wlroots allocator!");
        options.result.code = -1;
        return -1;
    }

    wlr_compositor_create(wlDisplay, 5, wlrRenderer);
    wlr_subcompositor_create(wlDisplay);
    wlr_data_device_manager_create(wlDisplay);

    wlrOutputLayout = wlr_output_layout_create(wlDisplay);

    wl_list_init(&outputs);

    eventListeners.newOutput.notify = newOutputEventBridge; 
    wl_signal_add(&wlrBackend->events.new_output, &eventListeners.newOutput);

    // scene

    scene = vesper::desktop::scene::Scene::create();
    if (!scene) {
        LOG_ERROR("failed to alloc scene!");
        options.result.code = -1;
        return -1;
    }
    
    sceneLayout = scene->attachWlrOutputLayout(wlrOutputLayout);
    if (!sceneLayout) {
        LOG_ERROR("failed to attach wlr output layout!");
        options.result.code = -1;
        return -1;
    }

    // xdg shell
    
    wl_list_init(&views);
    wlrXdgShell = wlr_xdg_shell_create(wlDisplay, 6);
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

    if (options.backend.headless && options.backend.virtualOutput.add) {
        uint32_t cap = WL_SEAT_CAPABILITY_POINTER;
        wlr_seat_set_capabilities(wlrSeat, cap);
    }

    // socket for Wayland display.

    const char* socket = wl_display_add_socket_auto(wlDisplay);
    if (!socket) {
        LOG_ERROR("failed to create wayland display socket!");
        wlr_backend_destroy(wlrBackend);
        options.result.code = -1;
        return -1;
    }

    if (!wlr_backend_start(wlrBackend)) {
        wlr_backend_destroy(wlrBackend);
        wl_display_destroy(wlDisplay);
        this->wlDisplay = nullptr;
        LOG_ERROR("failed to start wlr backend!");
        options.result.code = -1;
        return -1;
    }

    LOG_INFO("running wayland display with socket: ", socket);

    setenv("WAYLAND_DISPLAY", socket, true); 

    // launch apps

    for (auto& it : options.launch.apps) {
        pid_t pid = fork();
        if (pid == 0) { // new process
            execl("/bin/sh", "/bin/sh", "-c", it.c_str(), nullptr);
            exit(-1);
        } else if (pid < 0) { // failed 
            options.result.failedApps.push_back(it);
        }
    }

    LOG_INFO("server running...");

    options.result.code = 0;
    options.result.msg = "ok";

    // wayland event loop

    options.result.serverLaunchedSignal.release();

    wl_display_run(wlDisplay); // run blocking.

    // clean up.

    this->clear();

    return 0;
}

void Server::clear() {

    if (wlDisplay) {
        wl_display_destroy_clients(wlDisplay);
    }

    if (scene) {
        delete scene;
        scene = nullptr;
    }

    if (cursor) {
        delete cursor;
        cursor = nullptr;
    }

    if (wlDisplay) {
        wl_display_destroy(wlDisplay);
        wlDisplay = nullptr;
    }

    if (wlrSeat) {
        wlr_seat_destroy(wlrSeat);
        wlrSeat = nullptr;
    }
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


static int setResolutionAsyncCommandHandler(void* data) {
    Server* server = wl_container_of(data, server, setResolutionAsyncArgs);

    auto& args = server->setResolutionAsyncArgs;

    server->setResolutionAsyncArgsMutex.acquire();

    int currIdx = 0;
    int resCode = -1;
    Output* output;
    wl_list_for_each(output, &server->outputs, link) {
        if (args.index == currIdx++) {
            wlr_output_state state;
            wlr_output_state_init(&state);
            wlr_output_state_set_custom_mode(&state, args.width, args.height, args.refreshRate);
            
            if (wlr_output_commit_state(output->wlrOutput, &state)) {
                resCode = 0;
            }

            break;
        }
    }
    
    server->setResolutionAsyncArgsMutex.release();

    return 0;
}


int Server::setResolutionAsync() {
    wl_event_source* event = wl_event_loop_add_timer(
        wl_display_get_event_loop(wlDisplay), 
        setResolutionAsyncCommandHandler, 
        &setResolutionAsyncArgs
    );

    return wl_event_source_timer_update(event, 1);

    return 0;
}


static int moveCursorAsyncCommandHandler(void* data) {
    Server* server = wl_container_of(data, server, moveCursorAsyncArgs);
    
    auto& cursor = server->cursor;
    auto& args = server->moveCursorAsyncArgs;

    server->moveCursorAsyncArgsMutex.acquire();

    timespec currTime;
    clock_gettime(CLOCK_MONOTONIC, &currTime);

    wlr_seat_pointer_notify_frame(cursor->server->wlrSeat);

    wlr_cursor_warp_absolute(cursor->wlrCursor, nullptr, args.absoluteX, args.absoluteY);
    cursor->processMotion(currTime.tv_sec / 1000 + currTime.tv_nsec / 1000000);

    wlr_cursor_move(cursor->wlrCursor, nullptr, args.deltaX, args.deltaY);
    cursor->processMotion(currTime.tv_sec / 1000 + currTime.tv_nsec / 1000000);

    Output* output = wl_container_of(server->outputs.next, output, link);

    scene::Output* sceneOutput = server->scene->getSceneOutput(output->wlrOutput);
    
    sceneOutput->updateGeometry(true);

    server->moveCursorAsyncArgsMutex.release();

    return 0;
}


int Server::moveCursorAsync() {

    wl_event_source* event = wl_event_loop_add_timer(
        wl_display_get_event_loop(wlDisplay), 
        moveCursorAsyncCommandHandler, 
        &moveCursorAsyncArgs
    );

    return wl_event_source_timer_update(event, 1);
}


}
