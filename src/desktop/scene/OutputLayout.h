// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 显示屏布局
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "../../utils/ObjUtils.h"

namespace vesper::desktop::scene {

class Scene;
class Output;

class OutputLayout {

public:
    struct CreateOptions {
        Scene* scene;
        wlr_output_layout* wlrOutputLayout;
    };

    static OutputLayout* create(const CreateOptions&);
    
    ~OutputLayout();

    int addOutput(wlr_output_layout_output* wlrLayoutOutput, Output* sceneOutput);


protected:
    OutputLayout() {};
    VESPER_OBJ_UTILS_DISABLE_COPY(OutputLayout);
    int init(const CreateOptions&);

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


/**
 * 每个 OutputLayoutOutput (简称 olo) 与一个实际屏幕（也可能是虚拟屏幕）对应。
 * olo 负责监听并呈递与单个屏幕布局位置变化有关的 wayland 信号。
 */
class OutputLayoutOutput {

public:

    struct CreateOptions {
        Output* sceneOutput;
        wlr_output_layout_output* wlrLayoutOutput;
        OutputLayout* outputLayout;
    };

    static OutputLayoutOutput* create(const CreateOptions&);

    
    ~OutputLayoutOutput();

protected:
    OutputLayoutOutput() {}
    VESPER_OBJ_UTILS_DISABLE_COPY(OutputLayoutOutput)
    int init(const CreateOptions&);

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
