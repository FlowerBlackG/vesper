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
#include "./Popup.h"
#include "../scene/Scene.h"
#include "../scene/Output.h"
#include "../scene/FramebufferPlate.h"
#include "../scene/SceneNode.h"
#include "../scene/XdgShell.h"
#include "../scene/Surface.h"
#include "../../bindings/pixman.h"

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <thread>

#include <linux/input-event-codes.h>

using namespace std;
using namespace vesper::desktop;
using namespace vesper::common;
using namespace vesper::bindings;

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

    auto* wlrXdgPopup = (wlr_xdg_popup*) data;

    Popup* popup = Popup::create({
        .server = server,
        .xdgPopup = wlrXdgPopup
    });

    if (!popup) {
        LOG_ERROR("failed to allocate Popup object!");
        return;
    }
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

    auto* event = (wlr_seat_request_set_selection_event*) data;
    wlr_seat_set_selection(server->wlrSeat, event->source, event->serial);
}

Server::~Server() {
    
}

static void clearRunOptionsResult(Server::RunOptions& options) {
    
    auto& res = options.result;
    res.msg.clear();
    res.failedApps.clear();

    res.signals.serverLaunched.try_acquire();
    res.signals.firstDisplayAttached.try_acquire();
}

int Server::run() {
    
    clearRunOptionsResult(options);

    wl_signal_init(&events.destroy);

    this->wlDisplay = wl_display_create();
    this->terminated = false;
    this->wlEventLoop = wl_display_get_event_loop(wlDisplay);
    
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


    if (wlr_renderer_is_pixman(wlrRenderer)) {
        LOG_INFO("Vesper Desktop: renderer is pixman.")
    } else if (wlr_renderer_is_gles2(wlrRenderer)) {
        LOG_INFO("Vesper Desktop: renderer is gles2.")
    } else if (wlr_renderer_is_vk(wlrRenderer)) {
        LOG_INFO("Vesper Desktop: renderer is vulkan.")
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

    scene = vesper::desktop::scene::Scene::create({
        .desktopServer = this
    });
    
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
    setenv("XDG_SESSION_TYPE", "wayland", true);
    setenv("XDG_SESSION_DESKTOP", "Vesper", true);

    // launch apps

    for (auto& it : options.launch.apps) {
        pid_t pid = fork();
        
        if (pid == 0) { // new process
            
            execl("/bin/sh", "/bin/sh", "-c", it.c_str(), nullptr);
            exit(-1);

        } else if (pid > 0) { // parent process who has just created a child.

            this->childProcessList.push_back(pid);

        } else if (pid < 0) { // failed 

            options.result.failedApps.push_back(it);
        
        }
    }

    LOG_INFO("server running...");

    options.result.code = 0;
    options.result.msg = "ok";

    // wayland event loop

    options.result.signals.serverLaunched.release();
    if (options.runtimeCtrl.enabled) {
        this->runtimeControlEventLoop();  // run async
    }

    this->wlDisplayRunning = true;
    wl_display_run(wlDisplay); // run blocking.
    this->wlDisplayRunning = false;

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


    // send killing signals to children
    for (auto& it : this->childProcessList) {
        kill(it, SIGTERM);
    }
    this->childProcessList.clear();


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

    if (wlrAllocator) {
        wlr_allocator_destroy(wlrAllocator);
        wlrAllocator = nullptr;
    }

    if (wlrRenderer) {
        wlr_renderer_destroy(wlrRenderer);
        wlrRenderer = nullptr;
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


void Server::runtimeControlEventLoop() {
    if (this->terminated) {
        return;
    }

    // do things

    if (this->wlDisplayRunning) {
        runtimeControlCmds.lock.acquire();

        while (!runtimeControlCmds.waitingQueue.empty()) {
            auto& it = runtimeControlCmds.waitingQueue.front();
            it.f(this, it.data);  // invokes
            if (it.data) {
                delete it.data;
            }
            runtimeControlCmds.waitingQueue.pop();
        }

        runtimeControlCmds.lock.release();
    } // if (this->wlDisplayRunning)


    // register next loop

    if (!this->terminated) {
        wl_event_source* event = wl_event_loop_add_timer(
            this->wlEventLoop,
            [] (void* data) { 
                ((Server*) data)->runtimeControlEventLoop();
                return 0;
            },
            this
        );

        wl_event_source_timer_update(event, options.runtimeCtrl.checkIntervalMs);
    }
}


void* Server::getFramebuffer(int displayIndex, pixman::Region32& damage) {
    if (this->terminated) {
        return nullptr;
    }

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

    auto plate = serverOutput->sceneOutput->framebufferPlate;

    return plate->get(damage);
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

    this->currentOutput = output;  // todo: only 1 output supported.


    options.result.firstDisplayResolution.width = output->wlrOutput->width;
    options.result.firstDisplayResolution.height = output->wlrOutput->height;
    options.result.signals.firstDisplayAttached.release();
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


#define ADD_CMD_TO_QUEUE(callable) \
    runtimeControlCmds.lock.acquire(); \
    runtimeControlCmds.waitingQueue.emplace((RuntimeControlCmd) { \
        .f = callable, \
        .data = args \
    }); \
    runtimeControlCmds.lock.release(); 


int Server::setResolutionAsync(int index, int width, int height, int refreshRate) {
    if (!options.runtimeCtrl.enabled) {
        return -1;
    }

    auto* args = new (nothrow) SetResolutionAsyncArgs;
    if (!args) {
        LOG_ERROR("failed to alloc command for SetResolutionAsyncArgs!")
        return -1;
    }

    args->index = index;
    args->width = width;
    args->height = height;
    args->refreshRate = refreshRate;

    ADD_CMD_TO_QUEUE(
        [] (Server* server, void* untypedData) {
            auto data = (SetResolutionAsyncArgs*) untypedData;

            int currIdx = 0;
    
            Output* output;
            wl_list_for_each(output, &server->outputs, link) {
                if (data->index == currIdx++) {
                    wlr_output_state state;
                    wlr_output_state_init(&state);
                    wlr_output_state_set_custom_mode(&state, data->width, data->height, data->refreshRate);
                    
                    if (wlr_output_commit_state(output->wlrOutput, &state)) {
                        // success
                    }

                    break;
                }
            }
        }
    )

    return 0;
}


int Server::moveCursorAsync(
    bool absoulute, double absoluteX, double absoluteY, 
    bool delta, int deltaX, int deltaY
) { 
    if (!options.runtimeCtrl.enabled) {
        return -1;
    }

    auto* args = new (nothrow) MoveCursorAsyncArgs;
    if (!args) {
        LOG_ERROR("failed to alloc command for MoveCursorAsyncArgs!")
        return -1;
    }
    args->absolute = absoulute;
    args->absoluteX = absoluteX;
    args->absoluteY = absoluteY;
    args->delta = delta;
    args->deltaX = deltaX;
    args->deltaY = deltaY;


    ADD_CMD_TO_QUEUE(
        [] (Server* server, void* untypedData) {
            auto& cursor = server->cursor;
            auto* data = (MoveCursorAsyncArgs*) untypedData;
            auto& args = *data;

            auto currTime = currTimeMsec();

            wlr_cursor_warp_absolute(cursor->wlrCursor, nullptr, args.absoluteX, args.absoluteY);
            wlr_cursor_move(cursor->wlrCursor, nullptr, args.deltaX, args.deltaY);
            
            cursor->processMotion(currTime);

            wlr_seat_pointer_notify_frame(server->wlrSeat);

            Output* output = wl_container_of(server->outputs.next, output, link);
        }
    )

    return 0;
}



int Server::pressMouseButtonAsync(bool press, vesper::common::MouseButton button) {
    
    if (!options.runtimeCtrl.enabled) {
        return -1;
    }

    auto* args = new (nothrow) PressMouseButtonAsyncArgs;
    if (!args) {
        LOG_ERROR("failed to alloc command for MoveCursorAsyncArgs!")
        return -1;
    }
    args->press = press;
    args->button = button;

    ADD_CMD_TO_QUEUE(
        [] (Server* server, void* untypedData) {
            auto& args = * (PressMouseButtonAsyncArgs*) untypedData;

            uint32_t button = 0;

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
                    args.press ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED
                );
                wlr_seat_pointer_notify_frame(server->wlrSeat);
            }

        }
    )

    return 0;
}



int Server::scrollAsync(bool vertical, double delta, int32_t deltaDiscrete) {
    if (!options.runtimeCtrl.enabled) {
        return -1;
    }
    auto args = new (nothrow) ScrollAsyncArgs;
    if (!args) {
        LOG_ERROR("failed to alloc args for scrollAsync!")
        return -1;
    }

    args->vertical = vertical;
    args->delta = delta;
    args->deltaDiscrete = deltaDiscrete;

    ADD_CMD_TO_QUEUE(
        [] (Server* server, void* data) {

            auto& args = * (ScrollAsyncArgs*) data;
            wlr_seat_pointer_notify_axis(
                server->wlrSeat, currTimeMsec(), 
                args.vertical ? WL_POINTER_AXIS_VERTICAL_SCROLL : WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                args.delta, args.deltaDiscrete, WL_POINTER_AXIS_SOURCE_WHEEL,
                WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL
            );

            wlr_seat_pointer_notify_frame(server->wlrSeat);
        }
    )

    return 0;
}


int Server::keyboardInputAsync(xkb_keysym_t keysym, bool pressed) {
    if (!options.runtimeCtrl.enabled) { 
        return -1; 
    }
    auto args = new (nothrow) KeyboardInputAsyncArgs;
    if (!args) {
        LOG_ERROR("failed to alloc args for KeyboardInputAsync!")
        return -1;
    }

    args->keysym = keysym;
    args->pressed = pressed;

    const auto handler = [] (Server* server, void* untypedData) {
        auto& args = * (KeyboardInputAsyncArgs*) untypedData;

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
        } // if (scancode)
    };

    ADD_CMD_TO_QUEUE(handler);


    return 0;
}


int Server::terminateAsync() {
    if (!options.runtimeCtrl.enabled) {
        return -1;
    }

    RuntimeCtrlAsyncArgsBase* args = nullptr;
    ADD_CMD_TO_QUEUE(
        [] (Server* server, void*) {
            server->terminate();
        }
    )
    
    return 0;
}


#undef ADD_CMD_TO_QUEUE

/* ============ 运行时，外部传入控制信息 结束 ============ */


}  // namespace vesper::desktop::server

