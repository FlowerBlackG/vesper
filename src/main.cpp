// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Entry
 *
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#include "config.h"
#include "log/Log.h"
#include "compositor/Compositor.h"

#include "./utils/wlroots-cpp.h"

#include <iostream>
#include <iomanip>
#include <sstream>

#include <unistd.h>

#include <wlr/version.h>
#include <pixman-version.h>

using namespace std;
using namespace vesper;

static void printVersion() {
    LOG_INFO(
        "vesper version : ", 
        VESPER_VERSION_NAME, 
        " (", VESPER_VERSION_CODE,")"
    );

    LOG_INFO(
        "wlroots version: ",
        WLR_VERSION_STR
    );

    LOG_INFO(
        "pixman version : ",
        PIXMAN_VERSION_STRING
    );
}

int main(int argc, const char* argv[]) {

    printVersion();

    LOG_INFO("creating compositor")
    
    compositor::Compositor compositor;
    compositor.run();

    LOG_INFO("bye~");
    return 0;
}
