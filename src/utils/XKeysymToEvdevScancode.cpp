// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 根据 US ANSI 键盘布局，将 X Keysym 转换回 Keycode
 * 
 * 创建于 2024年2月15日 北京市海淀区
 */


#include "./XKeysymToEvdevScancode.h"
#include <map>
#include <set>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

using namespace std;

static map<xkb_keysym_t, uint32_t> keymap;
static set<xkb_keysym_t> modifierSet;
static bool ready = false;

static void buildKeymap() {

    /*

    下方映射表由代码辅助生成：
    
        #include <fstream>
        map<xkb_keysym_t, uint32_t> revMap;
        void dump() {
            auto out = ofstream("map.cpp", ios::binary | ios::out);
            for (auto& it : revMap) {
                out << "    keymap[" << it.first << "] = " << it.second << ";" << endl;
            }
            out.close();
        }

        ...

        static void onKeyPress(...) {
            ...
            if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
                if (revMap.count(sym)) {
                    // ignored.
                } else {
                    revMap[sym] = event->keycode; // 录入
                }
            }
            ...
        }

    */

    keymap[32] = 57;
    keymap[33] = 2;
    keymap[34] = 40;
    keymap[35] = 4;
    keymap[36] = 5;
    keymap[37] = 6;
    keymap[38] = 8;
    keymap[39] = 40;
    keymap[40] = 10;
    keymap[41] = 11;
    keymap[42] = 9;
    keymap[43] = 13;
    keymap[44] = 51;
    keymap[45] = 12;
    keymap[46] = 52;
    keymap[47] = 53;
    keymap[48] = 11;
    keymap[49] = 2;
    keymap[50] = 3;
    keymap[51] = 4;
    keymap[52] = 5;
    keymap[53] = 6;
    keymap[54] = 7;
    keymap[55] = 8;
    keymap[56] = 9;
    keymap[57] = 10;
    keymap[58] = 39;
    keymap[59] = 39;
    keymap[60] = 51;
    keymap[61] = 13;
    keymap[62] = 52;
    keymap[63] = 53;
    keymap[64] = 3;
    keymap[65] = 30;
    keymap[66] = 48;
    keymap[67] = 46;
    keymap[68] = 32;
    keymap[69] = 18;
    keymap[70] = 33;
    keymap[71] = 34;
    keymap[72] = 35;
    keymap[73] = 23;
    keymap[74] = 36;
    keymap[75] = 37;
    keymap[76] = 38;
    keymap[77] = 50;
    keymap[78] = 49;
    keymap[79] = 24;
    keymap[80] = 25;
    keymap[81] = 16;
    keymap[82] = 19;
    keymap[83] = 31;
    keymap[84] = 20;
    keymap[85] = 22;
    keymap[86] = 47;
    keymap[87] = 17;
    keymap[88] = 45;
    keymap[89] = 21;
    keymap[90] = 44;
    keymap[91] = 26;
    keymap[92] = 43;
    keymap[93] = 27;
    keymap[94] = 7;
    keymap[95] = 12;
    keymap[96] = 41;
    keymap[97] = 30;
    keymap[98] = 48;
    keymap[99] = 46;
    keymap[100] = 32;
    keymap[101] = 18;
    keymap[102] = 33;
    keymap[103] = 34;
    keymap[104] = 35;
    keymap[105] = 23;
    keymap[106] = 36;
    keymap[107] = 37;
    keymap[108] = 38;
    keymap[109] = 50;
    keymap[110] = 49;
    keymap[111] = 24;
    keymap[112] = 25;
    keymap[113] = 16;
    keymap[114] = 19;
    keymap[115] = 31;
    keymap[116] = 20;
    keymap[117] = 22;
    keymap[118] = 47;
    keymap[119] = 17;
    keymap[120] = 45;
    keymap[121] = 21;
    keymap[122] = 44;
    keymap[123] = 26;
    keymap[124] = 43;
    keymap[125] = 27;
    keymap[126] = 41;
    keymap[65288] = 14;
    keymap[65289] = 15;
    keymap[65293] = 28;
    keymap[65299] = 119;
    keymap[65300] = 70;
    keymap[65307] = 1;
    keymap[65360] = 102;
    keymap[65361] = 105;
    keymap[65362] = 103;
    keymap[65363] = 106;
    keymap[65364] = 108;
    keymap[65365] = 104;
    keymap[65366] = 109;
    keymap[65367] = 107;
    keymap[65379] = 110;
    keymap[65407] = 69;
    keymap[65421] = 96;
    keymap[65450] = 55;
    keymap[65451] = 78;
    keymap[65453] = 74;
    keymap[65454] = 83;
    keymap[65455] = 98;
    keymap[65456] = 82;
    keymap[65457] = 79;
    keymap[65458] = 80;
    keymap[65459] = 81;
    keymap[65460] = 75;
    keymap[65461] = 76;
    keymap[65462] = 77;
    keymap[65463] = 71;
    keymap[65464] = 72;
    keymap[65465] = 73;
    keymap[65470] = 59;
    keymap[65471] = 60;
    keymap[65472] = 61;
    keymap[65473] = 62;
    keymap[65474] = 63;
    keymap[65475] = 64;
    keymap[65476] = 65;
    keymap[65477] = 66;
    keymap[65478] = 67;
    keymap[65479] = 68;
    keymap[65480] = 87;
    keymap[65481] = 88;
    keymap[65505] = 42;
    keymap[65506] = 54;
    keymap[65507] = 29;
    keymap[65508] = 97;
    keymap[65509] = 58;
    keymap[65513] = 56;
    keymap[65514] = 100;
    keymap[65515] = 125;
    keymap[65516] = 126;
    keymap[65535] = 111;

}

static void buildModifierSet() {

    /*
    
    下方映射表由代码辅助生成：

        #include <fstream>
        #include <set>
        set<xkb_keysym_t> modifierSet;
        xkb_keysym_t prevKeysym;
        void dump() {
            auto out = ofstream("set.cpp", ios::binary | ios::out);
            for (auto& it : modifierSet) {
                out << "    modifierSet.insert(" << it << ");" << endl;
            }
            out.close();
        }

    */

    modifierSet.insert(65407);
    modifierSet.insert(65505);
    modifierSet.insert(65506);
    modifierSet.insert(65507);
    modifierSet.insert(65508);
    modifierSet.insert(65509);
    modifierSet.insert(65513);
    modifierSet.insert(65514);
    modifierSet.insert(65515);
    modifierSet.insert(65516);
}

static void prepare() {
    if (!ready) {
        buildKeymap();
        buildModifierSet();
        ready = true;
    }
}

uint32_t xKeysymToEvdevScancode(xkb_keysym_t keysym) {
    prepare();
    return keymap.count(keysym) ? keymap[keysym] : 0;
}

bool xKeysymIsModifier(xkb_keysym_t keysym) {
    return modifierSet.contains(keysym);
}


bool scancodeToXkbModIndex(
    uint32_t scancode, xkb_keymap* keymap, xkb_mod_index_t* idx, bool* isLockedKey
) {
    static xkb_keymap* prevKeymap = nullptr;
    static map<uint32_t, xkb_mod_index_t> code2idxMap;
    static set<uint32_t> lockedSet;

    if (prevKeymap != keymap) {
        code2idxMap.clear();
        lockedSet.clear();
        prevKeymap = keymap;
    }

    if (code2idxMap.contains(scancode)) {
        *isLockedKey = lockedSet.contains(scancode);
        *idx = code2idxMap[scancode];
        return true;
    }

    const char* modName = nullptr;
    bool isLock = false;
    
    switch (scancode) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT: {
            modName = XKB_MOD_NAME_SHIFT;
            break;
        }

        case KEY_LEFTALT:
        case KEY_RIGHTALT: {
            modName = XKB_MOD_NAME_ALT;
            break;
        }

        case KEY_LEFTMETA:
        case KEY_RIGHTMETA: {
            modName = XKB_MOD_NAME_LOGO;
            break;
        }

        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL: {
            modName = XKB_MOD_NAME_CTRL;
            break;
        }

        case KEY_NUMLOCK: {
            modName = XKB_MOD_NAME_NUM;
            isLock = true;
            break;
        }

        case KEY_CAPSLOCK: {
            modName = XKB_MOD_NAME_CAPS;
            isLock = true;
            break;
        }         

        default: {
            break;
        }
    }

    if (modName) {
        auto keyModIdx = xkb_map_mod_get_index(keymap, modName);
        if (keyModIdx != XKB_MOD_INVALID) {
            if (isLock) {
                lockedSet.insert(scancode);
            }
            code2idxMap[scancode] = keyModIdx;
        }

        *isLockedKey = isLock;
        *idx = keyModIdx;
        return true;
    }

    return false;
}
