// SPDX-License-Identifier: MulanPSL-2.0

/*
 * render data
 * 
 * 创建于 2024年2月5日 北京市海淀区
 */

#pragma once

#include "../../utils/wlroots-cpp.h"

namespace vesper::desktop::scene {

class Output;

struct RenderData {
    wl_output_transform transform;
    float scale;
    wlr_box logical;
    int transWidth, transHeight;

    Output* output;

    wlr_render_pass* wlrRenderPass;
    pixman_region32_t damage;
};

} // namespace vesper::desktop::scene
