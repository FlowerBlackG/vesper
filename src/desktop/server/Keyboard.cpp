// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 键盘
 * 
 * 创建于 2024年1月17日 广东省珠海市
 */

#include "./Server.h"
#include "./Keyboard.h"
#include "../../log/Log.h"

#include "../../utils/XKeysymToEvdevScancode.h"

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
    auto* event = (wlr_keyboard_key_event*) data;
    keyboard->keyEventHandler(
        event->state == WL_KEYBOARD_KEY_STATE_PRESSED, event->keycode, event->time_msec
    );
}

static void destroyEventBridge(wl_listener* listener, void* data) {
    Keyboard* keyboard = wl_container_of(listener, keyboard, eventListeners.destroy);
    delete keyboard;
}


static wlr_keyboard_impl puyiKeyboardImpl = {
    .name = "vesper Puyi keyboard",
    .led_update = [] (wlr_keyboard* keyboard, uint32_t leds) {}
};

/**
 * 溥仪键盘。 
 */
static wlr_keyboard* createPuyiKeyboard() {

    wlr_keyboard* keyboard = new (nothrow) wlr_keyboard;
    if (keyboard == nullptr) {
        return nullptr;
    }

    memset(keyboard, 0, sizeof(wlr_keyboard));

    wlr_keyboard_init(keyboard, &puyiKeyboardImpl, "vesper Puyi keyboard");
    return keyboard; // todo: 释放内存。
}


VESPER_OBJ_UTILS_IMPL_CREATE(Keyboard, Keyboard::CreateOptions)

int Keyboard::init(const CreateOptions& options) {
    
    this->server = options.server;

    if (options.wlrKeyboard) {
        this->wlrKeyboard = options.wlrKeyboard;
        wl_list_init(&eventListeners.puyiKeyboardDestroy.link);
    } else {
        this->wlrKeyboard = createPuyiKeyboard();
        if (wlrKeyboard == nullptr) {
            LOG_ERROR("failed to create Puyi Keyboard (virtual keyboard agent)!");
            return -1;
        }

        // let wlrSeat destroy puyi keyboard 

        eventListeners.puyiKeyboardDestroy.notify = [] (
            wl_listener* listener, void* data
        ) {
            Keyboard* keyboard = wl_container_of(
                listener, keyboard, eventListeners.puyiKeyboardDestroy
            );
            wlr_keyboard* wlrKeyboard = keyboard->wlrKeyboard;
            wl_signal_emit_mutable(
                &wlrKeyboard->base.events.destroy, nullptr
            );
            delete wlrKeyboard;

        };
        wl_signal_add(
            &server->wlrSeat->events.destroy, &eventListeners.puyiKeyboardDestroy
        );
    }
    
    
    // xkb keymap. 默认采用美式键盘布局。

    auto* xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    auto* xkbKeymap = xkb_keymap_new_from_names(xkbContext, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(wlrKeyboard, xkbKeymap);
    wlr_keyboard_set_repeat_info(wlrKeyboard, 25, 600);

    xkb_keymap_unref(xkbKeymap);
    xkb_context_unref(xkbContext);


    // 事件监听

    eventListeners.modifiers.notify = modifiersEventBridge;
    wl_signal_add(&wlrKeyboard->events.modifiers, &eventListeners.modifiers);

    eventListeners.key.notify = keyEventBridge;
    wl_signal_add(&wlrKeyboard->events.key, &eventListeners.key);

    eventListeners.destroy.notify = destroyEventBridge;
    wl_signal_add(&wlrKeyboard->base.events.destroy, &eventListeners.destroy);

    wlr_seat_set_keyboard(server->wlrSeat, wlrKeyboard);

    wl_list_insert(&server->keyboards, &link);

    return 0;
}

Keyboard::~Keyboard() {
    wl_list_remove(&eventListeners.modifiers.link);
    wl_list_remove(&eventListeners.key.link);
    wl_list_remove(&eventListeners.destroy.link);
    wl_list_remove(&eventListeners.puyiKeyboardDestroy.link);
    wl_list_remove(&link);
}

void Keyboard::keyEventHandler(bool pressed, uint32_t scancode, uint32_t timeMsec) {

    xkb_keysym_t keysym = xkb_state_key_get_one_sym(wlrKeyboard->xkb_state, scancode + 8);
    
    uint32_t modifiers = wlr_keyboard_get_modifiers(wlrKeyboard);

    bool handled = false;

    if ((modifiers & WLR_MODIFIER_ALT) && pressed) {
        // 将按键组合 “alt + ?” 视为 vesper 已定义键。
        
        if (keysym == XKB_KEY_q || keysym == XKB_KEY_Q) {
            handled = true;
            server->terminate();
        }
        
    }

    if (!handled) {
        // 如果 vesper 自己处理不了，就把它交给客户端处理～
        
        wlr_seat_set_keyboard(server->wlrSeat, wlrKeyboard);
        
        wlr_seat_keyboard_notify_key(
            server->wlrSeat, timeMsec, scancode, 
            pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED
        );
        
    } // if (!handled)

} // void Keyboard::keyEventHandler(bool pressed, xkb_keysym_t keysym, uint32_t timeMsec)

} // namespace vesper::desktop::server
