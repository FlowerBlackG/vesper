// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Entry
 *
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#include "config.h"
#include <iostream>
#include "log/Log.h"
#include <wlr/version.h>
using namespace std;

static void printVersion() {
    LOG_INFO(
        "vesper  version: ", 
        VESPER_VERSION_NAME, 
        " (", VESPER_VERSION_CODE,")"
    );

    LOG_INFO(
        "wlroots version: ",
        WLR_VERSION_STR
    );
}

int main(int argc, const char* argv[]) {

    printVersion();


    LOG_INFO("bye~");
    return 0;
}
