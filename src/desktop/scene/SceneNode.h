// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 场景树节点。
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 * 
 * 每个 Scene 内部都有一棵场景树。
 * 场景树上的节点分为三类：子树、简单矩形、缓冲数据
 * 
 * 其中，只有“子树”节点可以拥有子节点，且每个子树节点可以拥有多个子节点。
 * 简单矩形和缓冲数据仅能作为叶节点存在。
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include <new>
#include <functional>

namespace vesper::desktop::scene {

class Scene;
class Output;
class SceneTreeNode;

/**
 * 场景树节点类型。
 */
enum class SceneNodeType {
    TREE, BUFFER, RECT
};

/**
 * 场景树节点基类。
 */
class SceneNode {
public:
    virtual inline SceneNodeType type() = 0;

public:
    int init(SceneTreeNode* parent);
    Scene* getRootScene();

    void outputUpdate(
        wl_list* outputs, Output* ignore, Output* force
    );
    
    void updateNodeUpdateOutputs(wl_list* outputs, Output* ignore, Output* force);
    void update(pixman_region32_t* damage);

    bool coords(int* x, int* y);

    void setPosition(int x, int y);
    void setEnabled(bool enabled);

    void visibility(pixman_region32_t* visible);
    void bounds(int x, int y, pixman_region32_t* visible);

    void getSize(int* width, int* height);

    bool nodesInBox(
        wlr_box* box, 
        std::function<bool (SceneNode* node, int x, int y, void* data)> onDiscover, 
        void* data
    );

    void opaqueRegion(int x, int y, pixman_region32_t* opaque);

public:
    SceneNode* parent = nullptr;

    /**
     * 用于挂在父母节点的 children 列表里。
     */
    wl_list link;
    
    bool enabled = true;
    
    struct {
        int x = 0;
        int y = 0;
    } offset;
    
    struct {
        wl_signal destroy;
    } events;

    pixman_region32_t visibleArea;

    void* data;

public:
    void destroy(std::function<void(Scene*)> typeDestroy);

};

/**
 * 场景树 - 子树节点
 */
class SceneTreeNode : public SceneNode {
public:
    virtual inline SceneNodeType type() override { return SceneNodeType::TREE; }

    static SceneTreeNode* create(SceneTreeNode* parent);

    void destroy();
    int init(SceneTreeNode* parent);

public:
    /**
     * 所有孩子节点。children 列表挂的是子节点的 link 成员。
     */
    wl_list children;
};

/**
 * 场景树 - buffer 数据节点
 */
class SceneBufferNode : public SceneNode {
public:
    virtual inline SceneNodeType type() override { return SceneNodeType::BUFFER; }

    static SceneBufferNode* create(
        SceneTreeNode* parent, wlr_buffer* wlrBuffer
    ); 
    void destroy();
    int init(
        SceneTreeNode* parent, wlr_buffer* wlrBuffer
    );

public:
    void updateNodeUpdateOutputs(wl_list* outputs, Output* ignore, Output* force);
    void setBuffer(wlr_buffer* wlrBuffer);

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

    wlr_fbox srcBox;
    int dstWidth, dstHeight;

    pixman_region32_t opaqueRegion;
    uint64_t activeOutputs;
    wlr_texture* texture;
    
    bool ownBuffer;
    int bufferWidth, bufferHeight;
    bool bufferIsOpaque;
    
    struct {
        wl_listener bufferRelease;
    } eventListeners;
};

/**
 * 场景树 - 简单矩形节点
 */
class SceneRectNode : public SceneNode {
public:
    virtual inline SceneNodeType type() override { return SceneNodeType::RECT; }

    static SceneRectNode* create(
        SceneTreeNode* parent, int width, int height, const float color[4]
    ); 

    void destroy();
    int init(
        SceneTreeNode* parent, int width, int height, const float color[4]
    );

public:
    int width;
    int height;

    /**
     * 
     * RGBA
     */
    float color[4];
};



struct OutputsUpdateEvent {
    Output** active;
    size_t size;
};

}

