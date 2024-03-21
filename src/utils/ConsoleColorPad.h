// SPDX-License-Identifier: MulanPSL-2.0

/*

    命令行调色板。
    功能：改变文字输出的颜色。

    created on 2022.12.5
    from ToyCompile
*/

#pragma once

#include <iostream>


/**
 * 
 * 命令行调色板。
 * 功能：改变文字输出的颜色。
 * 
 * 如果我们觉得输出的字全都是白花花的，不好看，
 * 可以借助本工具进行调色，实现带颜色的文字输出。
 */
namespace ConsoleColorPad {


extern bool noColor;


/**
 * 设置调色板颜色。设置后，后续输出的字符会变成你设置的颜色。
 * 三原色的取值范围都是 [0, 255]。
 */
inline void setCoutColor(int red, int green, int blue) {
    if (!noColor) {
        std::cout << "\e[1;38;2;" << red << ";" << green << ";" << blue << "m";
    }
}

/**
 * 恢复控制台的默认颜色。
 */
inline void setCoutColor() {
    if (!noColor) {
        std::cout << "\e[0m";
    }
}

/**
 * 设置调色板颜色。设置后，后续输出的字符会变成你设置的颜色。
 * 三原色的取值范围都是 [0, 255]。
 */
inline void setClogColor(int red, int green, int blue) {
    if (!noColor) {
        std::clog << "\e[1;38;2;" << red << ";" << green << ";" << blue << "m";
    }
}

/**
 * 恢复控制台的默认颜色。
 */
inline void setClogColor() {
    if (!noColor) {
        std::clog << "\e[0m";
    }
}


inline void disableColor() {
    noColor = true;
}

inline void enableColor() {
    noColor = false;
}

inline void setNoColor(bool b) {
    noColor = b;
}

} // namespace ConsoleColorPad
