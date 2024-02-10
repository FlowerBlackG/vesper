// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 键盘
 * 
 * 创建于 2024年1月17日 广东省珠海市
 */

#include "./Server.h"
#include "./Keyboard.h"
#include "../../log/Log.h"

using namespace std;

namespace vesper::desktop::server {

/**
 * 当 alt ctrl 之类的按键被按下时触发。
 * 
 * @param listener 
 * @param data 
 */
static void modifiersEventBridge(wl_listener* listener, void* data) {

    Keyboard* keyboard = wl_container_of(listener, keyboard, eventListeners.modifiers);

    auto* server = keyboard->server;

    wlr_seat_set_keyboard(server->wlrSeat, keyboard->wlrKeyboard);

    // 向客户端发送信息。
    wlr_seat_keyboard_notify_modifiers(server->wlrSeat, &keyboard->wlrKeyboard->modifiers);
}

static void keyEventBridge(wl_listener* listener, void* data) {

    Keyboard* keyboard = wl_container_of(listener, keyboard, eventListeners.key);
    Server* server = keyboard->server;
    auto* event = (wlr_keyboard_key_event*) data;
    wlr_seat* seat = server->wlrSeat;

    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t* syms;
    int nsyms = xkb_state_key_get_syms(
        keyboard->wlrKeyboard->xkb_state, keycode, &syms
    );

    bool handled = false;

    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlrKeyboard);

    if ((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // 将按键组合 “alt + ?” 视为 vesper 已定义键。
        for (int i = 0; i < nsyms; i++) {
            if (syms[i] == XKB_KEY_q || syms[i] == XKB_KEY_Q) {
                handled = true;
                wl_display_terminate(server->wlDisplay);
            }
        }
    }

    if (!handled) {
        // 如果 vesper 自己处理不了，就把它交给客户端处理～
        
        wlr_seat_set_keyboard(seat, keyboard->wlrKeyboard);
        wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
    }
    
}

static void destroyEventBridge(wl_listener* listener, void* data) {
    Keyboard* keyboard = wl_container_of(listener, keyboard, eventListeners.destroy);

    wl_list_remove(&keyboard->eventListeners.modifiers.link);
    wl_list_remove(&keyboard->eventListeners.key.link);
    wl_list_remove(&keyboard->eventListeners.destroy.link);
    wl_list_remove(&keyboard->link);

    delete keyboard;
}

VESPER_OBJ_UTILS_IMPL_CREATE(Keyboard, Keyboard::CreateOptions)

int Keyboard::init(const CreateOptions& options) {
    
    this->server = options.server;
    this->wlrKeyboard = options.wlrKeyboard;

    // xkb keymap. 默认采用美式键盘布局。

    auto* xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    auto* xkbKeymap = xkb_keymap_new_from_names(xkbContext, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(wlrKeyboard, xkbKeymap);
    xkb_keymap_unref(xkbKeymap);
    xkb_context_unref(xkbContext);

    wlr_keyboard_set_repeat_info(wlrKeyboard, 25, 600);

    // 事件监听

    eventListeners.modifiers.notify = modifiersEventBridge;
    wl_signal_add(&wlrKeyboard->events.modifiers, &eventListeners.modifiers);

    eventListeners.key.notify = keyEventBridge;
    wl_signal_add(&wlrKeyboard->events.key, &eventListeners.key);

    eventListeners.destroy.notify = destroyEventBridge;
    wl_signal_add(&options.device->events.destroy, &eventListeners.destroy);

    wlr_seat_set_keyboard(server->wlrSeat, wlrKeyboard);

    wl_list_insert(&server->keyboards, &link);

    return 0;
}

}