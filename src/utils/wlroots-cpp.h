// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 快速 include wlr 系列头文件。
 * 解决 C++ 代码 include wlr 头文件时可能带来的编译报错。
 * 
 * 创建于 2023年12月27日 上海市嘉定区安亭镇
 */

#pragma once


#define WLR_USE_UNSTABLE

// 必须先加 wayland server core 这个头文件，否则 wlr 会无法编译。
#include <wayland-server-core.h>

extern "C" {
// wlr 用了 c 的一些语法，如 "[static 4]" 但是 cpp 不支持。
#define static 

    #include <wlr/types/wlr_compositor.h>
    #include <wlr/types/wlr_subcompositor.h>
    #include <wlr/types/wlr_data_device.h>
    #include <wlr/types/wlr_cursor.h>
    #include <wlr/types/wlr_keyboard.h>
    #include <wlr/types/wlr_input_device.h>
    #include <wlr/types/wlr_output.h>
    #include <wlr/types/wlr_output_layout.h>
    #include <wlr/types/wlr_pointer.h>
    #include <wlr/types/wlr_seat.h>
    #include <wlr/types/wlr_xcursor_manager.h>
    #include <wlr/types/wlr_damage_ring.h>
    #include <wlr/types/wlr_xdg_shell.h>
    #include <wlr/types/wlr_buffer.h>
    #include <wlr/types/wlr_presentation_time.h>
    #include <wlr/types/wlr_linux_dmabuf_v1.h>

    #include <wlr/types/wlr_fractional_scale_v1.h>

    #include <wlr/backend.h>
    
    #include <wlr/render/pixman.h>
    
    #include <wlr/render/wlr_renderer.h>
    #include <wlr/render/allocator.h>
    #include <wlr/render/swapchain.h>

    #include <wlr/util/transform.h>
    #include <wlr/util/region.h>
    #include <wlr/util/log.h>

    #include <wlr/backend/headless.h>
    #include <wlr/backend/session.h>

    #include <wlr/interfaces/wlr_output.h>
    #include <wlr/interfaces/wlr_keyboard.h>

#undef static
}


