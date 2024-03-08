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
#include "../../utils/XKeysymToEvdevScancode.h"
#include "../../common/MouseButton.h"
#include "./View.h"
#include "../scene/Scene.h"
#include "../scene/Output.h"
#include "../scene/SceneNode.h"
#include "../scene/XdgShell.h"
#include "../scene/Surface.h"

#include <unistd.h>
#include <thread>

#include <linux/input-event-codes.h>

using namespace std;
using namespace vesper::desktop;
using namespace vesper::common;

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
            .wlrKeyboard = wlrKeyboard
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

    wl_signal_init(&events.destroy);

    this->wlDisplay = wl_display_create();
    this->terminated = false;
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
        Keyboard::create({
            .server = this,
            .wlrKeyboard = nullptr
        });
        uint32_t cap = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
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

void Server::terminate() {
    if (terminated) {
        return;
    }

    terminated = true;
    wl_event_loop_add_idle(
        wl_display_get_event_loop(wlDisplay),
        [] (void* data) { wl_display_terminate( (wl_display*) data ); },
        this->wlDisplay
    );
}

void Server::clear() {

    wl_signal_emit_mutable(&events.destroy, nullptr);

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

    Keyboard* keyboard;
    Keyboard* keyboardTmp;
    wl_list_for_each_safe(keyboard, keyboardTmp, &this->keyboards, link) {
        delete keyboard;
    }
    
    if (wlrSeat) {
        wlr_seat_destroy(wlrSeat);
        wlrSeat = nullptr;
    }

    if (wlrBackend) {
        wlr_backend_destroy(wlrBackend);
        wlrBackend = nullptr;
    }
    
    
    if (wlDisplay) {
        wl_display_destroy(wlDisplay);
        wlDisplay = nullptr;
    }

}

void* Server::getFramebuffer(int displayIndex) {
    int currIdx = -1;
    Output* serverOutput;
    wl_list_for_each(serverOutput, &this->outputs, link) {
        currIdx++;
        if (currIdx == displayIndex) {
            break;
        }
    }

    if (displayIndex != currIdx) {
        return nullptr;
    }

    auto& plate = serverOutput->sceneOutput->framebufferPlate;

    wlr_buffer* wlrBuf = plate.get();
    if (!wlrBuf) {
        return nullptr;
    }

    pixman_image_t* img = wlr_pixman_renderer_get_buffer_image(
        serverOutput->wlrOutput->renderer, wlrBuf
    );

    if (!img) {
        plate.recycle(wlrBuf);
        return nullptr;
    }

    auto imgFormat = pixman_image_get_format(img);
    if (imgFormat != PIXMAN_a8r8g8b8 && imgFormat != PIXMAN_x8r8g8b8) {
        LOG_WARN("bad format: ", int64_t(imgFormat));
        plate.recycle(wlrBuf);
        return nullptr;
    }

    auto* data = pixman_image_get_data(img);
    framebufferRentMap[(void*) data] = wlrBuf;
    return data;
}

void Server::recycleFramebuffer(void* oldFrameData, int displayIndex) {
    if (!framebufferRentMap.contains(oldFrameData)) {
        LOG_WARN("frame data unrecognized!");
        return;
    }

    wlr_buffer* oldBuf = framebufferRentMap[oldFrameData];
    framebufferRentMap.erase(oldFrameData);

    int currIdx = -1;
    Output* serverOutput;
    wl_list_for_each(serverOutput, &this->outputs, link) {
        currIdx++;
        if (currIdx == displayIndex) {
            break;
        }
    }

    if (currIdx != displayIndex) {
        LOG_WARN("there's no display with index ", displayIndex);
        return;
    }

    serverOutput->sceneOutput->framebufferPlate.recycle(oldBuf);
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


/* ============ 运行时，外部传入控制信息 开始 ============ */

static inline int64_t currTimeMsec() {
    timespec currTime;
    clock_gettime(CLOCK_MONOTONIC, &currTime);
    return int64_t(currTime.tv_sec) * 1000 + currTime.tv_nsec / 1000000;
}

#if 0 // todo
#define IMPL_SERVER_ASYNC_COMMAND_BRIDGE(command) \
    int Server::command ## Async() { \
        wl_event_source* event = wl_event_loop_add_timer( \
            wl_display_get_event_loop(wlDisplay), \
            command ## AsyncCommandHandler, \
            &command ## AsyncArgs \
        ); \
        \
        return wl_event_source_timer_update(event, 1); \
    }
#else // todo : 优化性能
#define IMPL_SERVER_ASYNC_COMMAND_BRIDGE(command) \
    int Server::command ## Async() { \
        wl_event_source* event = wl_event_loop_add_timer( \
            wl_display_get_event_loop(wlDisplay), \
            command ## AsyncCommandHandler, \
            &command ## AsyncArgs \
        ); \
        \
        return wl_event_source_timer_update(event, 1); \
    }
#endif // todo


#define DECL_SERVER_ASYNC_COMMAND_HANDLER(command) \
    static int command ## AsyncCommandHandler(void* data) \

#define GET_SERVER(command) \
    Server* server = wl_container_of(data, server, command ## AsyncArgs);

#define GET_SERVER_AND_ARGS(command) \
    GET_SERVER(command) \
    auto& args = server->command ## AsyncArgs;



DECL_SERVER_ASYNC_COMMAND_HANDLER(setResolution) {
    GET_SERVER_AND_ARGS(setResolution)

    server->setResolutionAsyncArgsMutex.acquire();

    int currIdx = 0;
    
    Output* output;
    wl_list_for_each(output, &server->outputs, link) {
        if (args.index == currIdx++) {
            wlr_output_state state;
            wlr_output_state_init(&state);
            wlr_output_state_set_custom_mode(&state, args.width, args.height, args.refreshRate);
            
            if (wlr_output_commit_state(output->wlrOutput, &state)) {
                // success
            }

            break;
        }
    }
    
    server->setResolutionAsyncArgsMutex.release();

    return 0;
}

IMPL_SERVER_ASYNC_COMMAND_BRIDGE(setResolution)


DECL_SERVER_ASYNC_COMMAND_HANDLER(moveCursor) {
    GET_SERVER_AND_ARGS(moveCursor)
    auto& cursor = server->cursor;
    
    server->moveCursorAsyncArgsMutex.acquire();

    auto currTime = currTimeMsec();


    wlr_cursor_warp_absolute(cursor->wlrCursor, nullptr, args.absoluteX, args.absoluteY);
    wlr_cursor_move(cursor->wlrCursor, nullptr, args.deltaX, args.deltaY);
    
    cursor->processMotion(currTime);

    wlr_seat_pointer_notify_frame(server->wlrSeat);

    Output* output = wl_container_of(server->outputs.next, output, link);

    scene::Output* sceneOutput = server->scene->getSceneOutput(output->wlrOutput);
    
    sceneOutput->updateGeometry(true);

    server->moveCursorAsyncArgsMutex.release();

    return 0;
}

IMPL_SERVER_ASYNC_COMMAND_BRIDGE(moveCursor)


DECL_SERVER_ASYNC_COMMAND_HANDLER(pressMouseButton) {
    GET_SERVER_AND_ARGS(pressMouseButton)

    uint32_t button = 0;
    server->pressMouseButtonAsyncArgsMutex.acquire();

    if (args.button == MouseButton::LEFT) {
        button = BTN_LEFT;
    } else if (args.button == MouseButton::MIDDLE) {
        button = BTN_MIDDLE;
    } else if (args.button == MouseButton::RIGHT) {
        button = BTN_RIGHT;
    }


    if (button) {
        server->cursor->buttonEventHandler(
            currTimeMsec(), 
            button, 
            args.press ? WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED
        );
        wlr_seat_pointer_notify_frame(server->wlrSeat);
    }
    
    server->pressMouseButtonAsyncArgsMutex.release();

    return 0;
}


IMPL_SERVER_ASYNC_COMMAND_BRIDGE(pressMouseButton)


DECL_SERVER_ASYNC_COMMAND_HANDLER(scroll) {
    GET_SERVER_AND_ARGS(scroll)
    server->scrollAsyncArgsMutex.acquire();

    wlr_seat_pointer_notify_axis(
        server->wlrSeat, currTimeMsec(), 
        args.vertical ? WLR_AXIS_ORIENTATION_VERTICAL : WLR_AXIS_ORIENTATION_HORIZONTAL,
        args.delta, args.deltaDiscrete, WLR_AXIS_SOURCE_WHEEL,
        WLR_AXIS_RELATIVE_DIRECTION_IDENTICAL
    );

    server->scrollAsyncArgsMutex.release();

    wlr_seat_pointer_notify_frame(server->wlrSeat);

    return 0;
}

IMPL_SERVER_ASYNC_COMMAND_BRIDGE(scroll)


DECL_SERVER_ASYNC_COMMAND_HANDLER(keyboardInput) {
    GET_SERVER_AND_ARGS(keyboardInput)

    server->keyboardInputAsyncArgsMutex.acquire();

    auto scancode = xKeysymToEvdevScancode(args.keysym);
    wl_keyboard_key_state pressState = WL_KEYBOARD_KEY_STATE_RELEASED;
    if (args.pressed) {
        pressState = WL_KEYBOARD_KEY_STATE_PRESSED;
    }

    if (scancode) {
        wlr_keyboard_key_event event {
            .time_msec = uint32_t(currTimeMsec()),
            .keycode = scancode,
            .update_state = false,
            .state = pressState,
        };

        // 只考虑第一个键盘。
        Keyboard* keyboard = wl_container_of(server->keyboards.next, keyboard, link);
        auto& wlrKeyboard = keyboard->wlrKeyboard;

        wlr_keyboard_notify_key(wlrKeyboard, &event);
        
        // modifiers
        
        xkb_mod_mask_t depressed = xkb_state_serialize_mods(
            wlrKeyboard->xkb_state, XKB_STATE_MODS_DEPRESSED
        );
        xkb_mod_mask_t latched = xkb_state_serialize_mods(
            wlrKeyboard->xkb_state, XKB_STATE_MODS_LATCHED
        );
        xkb_mod_mask_t locked = xkb_state_serialize_mods(
            wlrKeyboard->xkb_state, XKB_STATE_MODS_LOCKED
        );

        bool isLock = false;
        xkb_mod_index_t keyModIdx;

        if (scancodeToXkbModIndex(scancode, wlrKeyboard->keymap, &keyModIdx, &isLock)) {

            xkb_mod_mask_t mask = 1 << keyModIdx;

            if (args.pressed) {
                depressed |= mask;
                if (isLock) {
                    locked ^= mask;
                }
            } else {
                depressed &= ~mask;
            }

            wlr_keyboard_notify_modifiers(
                wlrKeyboard, depressed, latched, locked, 
                wlrKeyboard->modifiers.group
            );
        }
    }


    server->keyboardInputAsyncArgsMutex.release();

    return 0;
}

IMPL_SERVER_ASYNC_COMMAND_BRIDGE(keyboardInput)

DECL_SERVER_ASYNC_COMMAND_HANDLER(terminate) {
    GET_SERVER(terminate)
    server->terminate();
    return 0;
}

IMPL_SERVER_ASYNC_COMMAND_BRIDGE(terminate)

#undef IMPL_SERVER_ASYNC_COMMAND_BRIDGE
#undef DECL_SERVER_ASYNC_COMMAND_HANDLER
#undef GET_SERVER_AND_ARGS
#undef GET_SERVER

/* ============ 运行时，外部传入控制信息 结束 ============ */

}
