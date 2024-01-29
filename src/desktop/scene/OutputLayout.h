// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 显示屏布局
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#pragma once

#include "../../utils/wlroots-cpp.h"

namespace vesper::desktop::scene {

class Scene;
class Output;

class OutputLayout {

public:
    int init(Scene* scene, wlr_output_layout* wlrOutputLayout);
    void destroy();


public:
    wlr_output_layout* wlrLayout;
    Scene* scene;

    /**
     * 成员类型：OutputLayoutOutput
     */
    wl_list outputs;

    struct {
        wl_listener layoutChange;
        wl_listener layoutDestroy;
        wl_listener sceneDestroy;
    } eventListeners;

}; // class OutputLayout

class OutputLayoutOutput {

public:
    void destroy();

public:

    wlr_output_layout_output* wlrLayoutOutput;

    Output* sceneOutput;

    /**
     * 用于挂到 OutputLayout 的 outputs 列表。
     */
    wl_list link;

    struct {
        wl_listener layoutOutputDestroy;
        wl_listener sceneOutputDestroy;
    } eventListeners;

}; // class OutputLayoutOutput

}
