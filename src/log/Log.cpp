// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 日志工具
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#include "Log.h"

#include <ctime>

using namespace std;

namespace vesper {
namespace log {
    
inline static void printLineInfo(const char* filename, int line) {
    clog << "[" 
        << filename 
        << " " 
        << line 
        << "]";
}

inline static void printCurrentTime() {
    time_t now = time(nullptr);
    tm* currTm = localtime(&now);
    char timeStr[32] = "";
    strftime(
        timeStr, 
        sizeof(timeStr) / sizeof(char), 
        "%Y-%m-%d %H:%M:%S", 
        currTm
    );
    clog << timeStr;
}

void printInfo(const char* filename, int line) {
    printCurrentTime();
    clog << "  ";
    printLineInfo(filename, line);
    clog.put(' ');
}

}
}