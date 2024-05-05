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
#include "../../utils/ObjUtils.h"
#include "../../bindings/pixman.h"
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
    Scene* getRootScene();
    int init(SceneTreeNode* parent);

    void outputUpdate(
        wl_list* outputs, Output* ignore, Output* force
    );
    
    void update(pixman_region32_t* damage);

    bool coords(int* x, int* y);

    void setPosition(int x, int y);
    void setEnabled(bool enabled);

    void visibility(pixman_region32_t* visible);
    void bounds(int x, int y, pixman_region32_t* visible);

    void getSize(int* width, int* height);

    /**
     * 搜索给定区域，寻找该区域内是否存在叶节点，并为遍历到的叶节点调用 onDiscover 处理方法。
     * 
     * @param box 搜索区域
     * 
     * @param onDiscover 当发现一个子节点时，调用。
     *                   注意，并不是每个叶节点都会被遍历到。当 onDiscover 返回 true 时，
     *                   遍历过程会被提前结束。如果需要强制遍历所有叶节点，onDiscover 应
     *                   总是返回 false。 
     * 
     * @param data 可以额外携带的信息。将会原封不动交给 onDiscover 方法。
     * 
     * @return 区域内是否存在叶节点。
     */
    bool nodesInBox(
        wlr_box* box, 
        std::function<bool (SceneNode* node, int x, int y, void* data)> onDiscover, 
        void* data
    );

    void opaqueRegion(int x, int y, pixman_region32_t* opaque);

    virtual bool invisible() = 0;

    void sendFrameDone(Output* sceneOutput, timespec* now);

    void placeAbove(SceneNode* sibling);
    void placeBelow(SceneNode* sibling);

    void raiseToTop();
    void lowerToBottom();

    SceneNode* nodeAt(double lx, double ly, double* nx, double* ny);

protected:
    SceneNode() {}
    VESPER_OBJ_UTILS_DISABLE_COPY(SceneNode);

public:
    SceneTreeNode* parent = nullptr;

    /**
     * 用于挂在父母节点的 children 列表里。
     */
    wl_list link;
    
    bool enabled = true;

    // whether ignore mouse and touch
    bool inputTransparent = false;
    
    struct {
        int x = 0;
        int y = 0;
    } offset;
    
    struct {
        wl_signal destroy;
    } basicEvents;

    vesper::bindings::pixman::Region32 visibleArea;

    void* data = nullptr;

    wlr_addon_set addons;

public:
    virtual ~SceneNode();

};

/**
 * 场景树 - 子树节点
 */
class SceneTreeNode : public SceneNode {
public:
    virtual inline SceneNodeType type() override { return SceneNodeType::TREE; }

    static SceneTreeNode* create(SceneTreeNode* parent);

    ~SceneTreeNode();
    int init(SceneTreeNode* parent);

    virtual inline bool invisible() override { return true; };

public:
    /**
     * 所有孩子节点。children 列表挂的是子节点的 link 成员。
     * 从头向后遍历，越靠后的节点在屏幕上的位置越靠上。
     * 
     * 例如，children 如下时：
     *   children -> 1 -> 2 -> 3 -> 循环
     * 节点 3 是在最上层的。节点 1 的内容可能被 2 和 3 遮挡。
     */
    wl_list children;

    /**
     * 只有当当前节点是跟节点时，才允许该指针指向 Scene。
     */
    Scene* rootScene = nullptr;
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
    ~SceneBufferNode();
    int init(
        SceneTreeNode* parent, wlr_buffer* wlrBuffer
    );

    virtual inline bool invisible() override;

public:
    void updateNodeUpdateOutputs(wl_list* outputs, Output* ignore, Output* force);
    void setBuffer(wlr_buffer* wlrBuffer, pixman_region32_t* damage = nullptr);
    wlr_texture* getTexture(wlr_renderer* wlrRenderer);

    void unmarkClientBuffer();

    void setOpaqueRegion(pixman_region32_t* opaque);
    void setOpaqueRegion(vesper::bindings::pixman::Region32& opaque);
    void setSourceBox(wlr_fbox* srcBox);
    void setDstSize(int width, int height);
    void setTransform(wl_output_transform transform);


public:
    wlr_buffer* wlrBuffer = nullptr;

    struct {
        wl_signal outputsUpdate;
        wl_signal outputEnter;
        wl_signal outputLeave;
        wl_signal outputSample;
        wl_signal frameDone;
    } events;

    std::function<bool (SceneBufferNode*, double* sx, double* sy)> pointAcceptsInput;

    Output* primaryOutput = nullptr;

    float opacity = 0;

    wlr_fbox srcBox;
    int dstWidth = 0, dstHeight = 0;

    wl_output_transform transform;

    vesper::bindings::pixman::Region32 opaqueRegion;
    uint64_t activeOutputs = 0;
    wlr_texture* texture = nullptr;

    wlr_linux_dmabuf_feedback_v1_init_options dmaBufPrevFeedbackOptions;
    
    bool ownBuffer;
    int bufferWidth = 0, bufferHeight = 0;
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

    ~SceneRectNode();
    int init(
        SceneTreeNode* parent, int width, int height, const float color[4]
    );

    virtual inline bool invisible() override;

public:
    int width = 0;
    int height = 0;

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

