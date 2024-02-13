// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 场景管理（窗口管理）
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "../../bindings/pixman.h"

namespace vesper::desktop::scene {

class SceneTreeNode;
class SceneBufferNode;
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

    ~Scene();
    

public:
    int init();

    OutputLayout* attachWlrOutputLayout(wlr_output_layout* wlrOutputLayout);

    void destroyTree();

    void updateRegion(vesper::bindings::pixman::Region32& updateRegion);
    void updateRegion(pixman_region32_t* updateRegion);
    void damageOutputs(vesper::bindings::pixman::Region32& damage);
    void damageOutputs(pixman_region32_t* damage);

    Output* getSceneOutput(wlr_output* output);

    void setLinuxDmaBufV1(wlr_linux_dmabuf_v1* linuxDmaBufV1);
    void sceneBufferSendDmaBufFeedback(
        SceneBufferNode* sceneBuffer,
        wlr_linux_dmabuf_feedback_v1_init_options* options
    );


public:

    SceneTreeNode* tree = nullptr;

    /**
     * 成员类型：SceneOutput
     */
    wl_list outputs;

    bool calculateVisibility;

    /**
     * 
     * nullable
     */
    wlr_linux_dmabuf_v1* linuxDmaBufV1 = nullptr;

    struct {
        wl_listener linuxDmaBufV1Destroy;
    } eventListeners;

};

}
