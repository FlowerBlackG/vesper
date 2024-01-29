// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 场景树节点。
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include <new>
#include <functional>

namespace vesper::desktop::scene {

class Scene;

enum class SceneNodeType {
    TREE, BUFFER, RECT
};

class SceneNode {
public:
    virtual inline SceneNodeType type() = 0;

public:
    int init(SceneNode* parent);
    Scene* getRootScene();
    void outputUpdate(); // todo

public:
    SceneNode* parent = nullptr;

    /**
     * 用于挂在父母节点的 children 列表里。
     */
    wl_list link;

    /**
     * 所有孩子节点。children 列表挂的是子节点的 link 成员。
     */
    wl_list children;
    
    bool enabled = true;
    
    struct {
        int x = 0;
        int y = 0;
    } offset;
    
    struct {
        wl_signal destroy;
    } events;

    pixman_region32_t visibleArea;

public:
    void destroy(std::function<void(Scene*)> typeDestroy);

protected:
    template<typename T>
    static inline T* create() {
        T* p = new (std::nothrow) T;
        if (!p) { return nullptr; }
        if (p->init()) { delete p; return nullptr; }
        return p;
    };

};

class SceneTreeNode : public SceneNode {
public:
    virtual inline SceneNodeType type() override { return SceneNodeType::TREE; }

    static inline SceneTreeNode* create() { return SceneNode::create<SceneTreeNode>(); }
    void destroy();
    int init();
};

class SceneBufferNode : public SceneNode {
public:
    virtual inline SceneNodeType type() override { return SceneNodeType::BUFFER; }

    static inline SceneBufferNode* create() { return SceneNode::create<SceneBufferNode>(); }
    void destroy();
    int init();

public:
    wlr_buffer* wlrBuffer = nullptr;

    struct {
        wl_signal outputsUpdate;
        wl_signal outputEnter;
        wl_signal outputLeave;
        wl_signal outputSample;
        wl_signal frameDone;
    } events;

    // todo: wlr_scene_buffer_point_accepts_input_func_t point_accepts_input;

    Output* primaryOutput;

    float opacity;
    // todo
};

class SceneRectNode : public SceneNode {
public:
    virtual inline SceneNodeType type() override { return SceneNodeType::RECT; }

    static inline SceneRectNode* create() { return SceneNode::create<SceneRectNode>(); }
    void destroy();
    int init();
};


}

