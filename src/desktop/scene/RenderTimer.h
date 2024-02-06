// SPDX-License-Identifier: MulanPSL-2.0

/*
 * render timer
 * 
 * 创建于 2024年2月5日 北京市海淀区
 */

#pragma once

#include "../../utils/wlroots-cpp.h"

namespace vesper::desktop::scene {

struct RenderTimer {
    int64_t preRenderDuration;
    wlr_render_timer* wlrRenderTimer;
};

} // namespace vesper::desktop::scene
