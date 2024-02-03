// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 场景管理（窗口管理）
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#pragma once

#include "../../utils/wlroots-cpp.h"

namespace vesper::desktop::scene {

class SceneTreeNode;
class Output;
class OutputLayout;

class Scene {

public:
    /**
     * Create a Scene object.
     * This will alloc the object as well as init it by calling init().
     * 
     * @return nullptr if failed. 
     */
    static Scene* create();

public:
    int init();

    OutputLayout* attachWlrOutputLayout(wlr_output_layout* wlrOutputLayout);

    void destroyTree();

    void updateRegion(pixman_region32_t* updateRegion);
    void damageOutputs(pixman_region32_t* damage);


public:
    SceneTreeNode* tree = nullptr;

    /**
     * 成员类型：SceneOutput
     */
    wl_list outputs;

    bool calculateVisibility;

};

}
