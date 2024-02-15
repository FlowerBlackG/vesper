// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 根据 US ANSI 键盘布局，将 X Keysym 转换回 Keycode
 * 
 * 创建于 2024年2月15日 北京市海淀区
 */

#pragma once

#include <cstdint>
#include <xkbcommon/xkbcommon.h>

std::uint32_t xKeysymToEvdevScancode(xkb_keysym_t keysym);
bool xKeysymIsModifier(xkb_keysym_t keysym);
bool scancodeToXkbModIndex(
    std::uint32_t scancode, xkb_keymap* keymap, xkb_mod_index_t* idx, bool* isLockedKey
);
