// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 日志工具
 *
 * 日志格式：
 * [main.cpp 32] xxx
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#pragma once

#include <iostream>
#include <cstdlib>
#include "../utils/ConsoleColorPad.h"


namespace vesper {
namespace log {

void printInfo(const char* filename, int line);

inline void printContent() {
    std::clog << std::endl;
}

template <typename T, typename... Args>
inline void printContent(T value, Args... args) {
    std::clog << value;
    printContent(args...);
}

inline void resetColor() {
    ConsoleColorPad::setClogColor();
}

inline void setColorInfo() {
    ConsoleColorPad::setClogColor(255, 255, 255);
}

inline void setColorWarn() {
    ConsoleColorPad::setClogColor(255, 192, 64);
}

inline void setColorError() {
    ConsoleColorPad::setClogColor(255, 64, 64);
}


}
}

// file name 这个宏应该是可以用的，但是编辑器容易误报错。这里假装再定义一下。
#ifndef __FILE_NAME__
    #define __FILE_NAME__ ""
#endif

#define LOG_PLAIN(...) \
    { \
        vesper::log::printInfo( \
            __FILE_NAME__, __LINE__ \
        ); \
        vesper::log::printContent(__VA_ARGS__); \
    }

#define LOG_INFO(...) \
    { \
        vesper::log::setColorInfo(); \
        LOG_PLAIN(__VA_ARGS__); \
        vesper::log::resetColor(); \
    }

#define LOG_ERROR(...) \
    { \
        vesper::log::setColorError(); \
        LOG_PLAIN(__VA_ARGS__); \
        vesper::log::resetColor(); \
    }

#define LOG_WARN(...) \
    { \
        vesper::log::setColorWarn(); \
        LOG_PLAIN(__VA_ARGS__); \
        vesper::log::resetColor(); \
    }

#if 1
    #define LOG_TEMPORARY(...) { \
        ConsoleColorPad::setClogColor(128, 128, 255); \
        LOG_PLAIN(__VA_ARGS__); \
        vesper::log::resetColor(); \
    }
#endif

#if 1
    #define TODO(...) { \
        ConsoleColorPad::setClogColor(255, 192, 192); \
        LOG_PLAIN("TODO: ", __VA_ARGS__); \
        vesper::log::resetColor(); \
        exit(-1); \
    }
#endif
