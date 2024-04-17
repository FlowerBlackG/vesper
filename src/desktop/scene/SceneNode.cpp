// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 场景树节点。
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#include "./SceneNode.h"
#include "./Scene.h"
#include "./Output.h"
#include "../../log/Log.h"
#include "../../bindings/pixman/Region32.h"

#include <functional>

using namespace std;
using namespace vesper::bindings;

namespace vesper::desktop::scene {

/* ------------ SceneNode 公共方法 开始 ------------ */

int SceneNode::init(SceneTreeNode* parent) {

    this->parent = parent;
    wl_signal_init(&basicEvents.destroy);
    wl_list_init(&link);

    if (parent) {
        wl_list_insert(parent->children.prev, &link); // 尾插。
    }

    wlr_addon_set_init(&this->addons);

    return 0;
}


SceneNode::~SceneNode() {
    wl_signal_emit_mutable(&basicEvents.destroy, nullptr);
    wlr_addon_set_finish(&this->addons);
    wl_list_remove(&link);
}


Scene* SceneNode::getRootScene() {
    SceneNode* node = this;
    while (node->parent) {
        node = node->parent;
    }

    Scene* scene = ((SceneTreeNode*) node)->rootScene;
    return scene;
}


void SceneNode::outputUpdate(
    wl_list* outputs, Output* ignore, Output* force
) {
    if (type() == SceneNodeType::TREE) {
        auto* tree = (SceneTreeNode*) this;
        SceneNode* child;
        wl_list_for_each(child, &tree->children, link) {
            child->outputUpdate(outputs, ignore, force);
        }
    } else if (type() == SceneNodeType::BUFFER) {
        ((SceneBufferNode*) this)->updateNodeUpdateOutputs(outputs, ignore, force);
    }
}


void SceneNode::update(pixman_region32_t* damage) {
    Scene* scene = this->getRootScene();
    int x, y;

    if (!this->coords(&x, &y)) {
        if (damage) {
            scene->updateRegion(damage);
            scene->damageOutputs(damage);
            pixman_region32_fini(damage);
        }

        return;
    }

    pixman_region32_t visible;

    if (!damage) {
        pixman_region32_init(&visible);
        this->visibility(&visible);
        damage = &visible;
    }

    pixman::Region32 updateRegion = damage;
    
    this->bounds(x, y, updateRegion.raw());

    scene->updateRegion(updateRegion);

    this->visibility(damage);
    scene->damageOutputs(damage);
    pixman_region32_fini(damage);
}


bool SceneNode::coords(int* x, int* y) {
    int lx = 0, ly = 0;
    bool enabled = true;

    auto* node = this;

    while (true) {
        lx += node->offset.x;
        ly += node->offset.y;
        enabled = enabled && node->enabled;

        if (node->parent == nullptr) {
            break;
        }

        node = node->parent;
    }

    *x = lx;
    *y = ly;
    return enabled;
}


void SceneNode::setPosition(int x, int y) {
    if (this->offset.x == x && this->offset.y == y) {
        return;
    }

    this->offset.x = x;
    this->offset.y = y;

    this->update(nullptr);
}


void SceneNode::setEnabled(bool enabled) {
    if (this->enabled == enabled) {
        return;
    }

    int x, y;
    pixman_region32_t visible;
    pixman_region32_init(&visible);

    if (this->coords(&x, &y)) {
        this->visibility(&visible);
    }

    this->enabled = enabled;
    this->update(&visible);
}


void SceneNode::visibility(pixman_region32_t* visible) {
    if (!enabled) {
        return;
    }

    if (type() == SceneNodeType::TREE) {
        auto* tree = reinterpret_cast<SceneTreeNode*>(this);
        SceneNode* child;
        wl_list_for_each(child, &tree->children, link) {
            child->visibility(visible);
        }
    } else { // buffer or rect
        pixman_region32_union(visible, visible, visibleArea.raw());
    }
}


void SceneNode::bounds(int x, int y, pixman_region32_t* visible) {
    if (!enabled) {
        return;
    }

    if (type() == SceneNodeType::TREE) {
        auto* tree = (SceneTreeNode*) this;
        SceneNode* child;
        wl_list_for_each(child, &tree->children, link) {
            child->bounds(x + child->offset.x, y + child->offset.y, visible);
        }
    } else {
        int width, height;
        this->getSize(&width, &height);
        pixman_region32_union_rect(visible, visible, x, y, width, height);
    }
}


void SceneNode::getSize(int* width, int* height) {
    *width = *height = 0;

    switch (type()) {
        case SceneNodeType::TREE: {
            return;
        }

        case SceneNodeType::RECT: {
            auto* rect = (SceneRectNode*) this;
            *width = rect->width;
            *height = rect->height;
            break;
        }

        case SceneNodeType::BUFFER: {
            auto* buf = (SceneBufferNode*) this;
            if (buf->dstHeight > 0 && buf->dstWidth > 0) {
                *width = buf->dstWidth;
                *height = buf->dstHeight;
            } else {
                *width = buf->bufferWidth;
                *height = buf->bufferHeight;
            }
            break;
        }
    } // switch (type())
} // void SceneNode::getSize


static inline bool nodesInBoxRecursion(
    SceneNode* node, 
    wlr_box* box,
    std::function<bool (SceneNode* node, int x, int y, void* data)> onDiscover,
    void* data,
    int x, 
    int y
) {
    if (!node->enabled) { 
        return false; 
    }

    if (node->type() == SceneNodeType::TREE) {
        auto* tree = (SceneTreeNode*) node;
        SceneNode* child;
        wl_list_for_each_reverse(child, &tree->children, link) {
            bool inBox = nodesInBoxRecursion(
                child, box, onDiscover, data, 
                child->offset.x + x, child->offset.y + y
            );

            if (inBox) {
                return true;
            }
        }
    } else { // rect or buffer
        wlr_box nodeBox = {
            .x = x,
            .y = y
        };

        node->getSize(&nodeBox.width, &nodeBox.height);

        bool intersect = wlr_box_intersection(&nodeBox, &nodeBox, box);
        if (intersect && onDiscover(node, x, y, data)) {
            return true;
        }
    }

    return false;
}


bool SceneNode::nodesInBox(
    wlr_box* box, 
    std::function<bool (SceneNode* node, int x, int y, void* data)> onDiscover, 
    void* data
) {

    int x, y;
    this->coords(&x, &y);

    return nodesInBoxRecursion(this, box, onDiscover, data, x, y);

}


void SceneNode::opaqueRegion(int x, int y, pixman_region32_t* opaque) {
    int width, height;
    this->getSize(&width, &height);

    if (type() == SceneNodeType::RECT) {
        auto* rect = (SceneRectNode*) this;
        if (rect->color[3] != 1) {
            return;
        }
    } else if (type() == SceneNodeType::BUFFER) {
        auto* buf = (SceneBufferNode*) this;

        if (!buf->wlrBuffer) {
            return;
        }

        if (buf->opacity != 1) {
            return;
        }

        if (!buf->bufferIsOpaque) {
            pixman_region32_copy(opaque, buf->opaqueRegion.raw());
            pixman_region32_intersect_rect(opaque, opaque, 0, 0, width, height);
            pixman_region32_translate(opaque, x, y);
            return;
        }
    }

    pixman_region32_fini(opaque);
    pixman_region32_init_rect(opaque, x, y, width, height);
}


void SceneNode::sendFrameDone(Output* sceneOutput, timespec* now) {
    if (!enabled) {
        return;
    }

    if (type() == SceneNodeType::BUFFER) {

        auto* p = (SceneBufferNode*) this;
        if (p->primaryOutput == sceneOutput) {
            if (p->visibleArea.notEmpty()) {
                wl_signal_emit_mutable(&p->events.frameDone, now);
            }
        }

    } else if (type() == SceneNodeType::TREE) {
        auto* p = (SceneTreeNode*) this;
        SceneNode* child;
        wl_list_for_each(child, &p->children, link) {
            child->sendFrameDone(sceneOutput, now);
        }
    }
}


void SceneNode::placeAbove(SceneNode* sibling) {

    if (link.prev == &sibling->link) {
        return;
    }

    wl_list_remove(&this->link);
    wl_list_insert(&sibling->link, &this->link);
    this->update(nullptr);
}


void SceneNode::placeBelow(SceneNode* sibling) {

    if (link.next == &sibling->link) {
        return;
    }

    wl_list_remove(&this->link);
    wl_list_insert(sibling->link.prev, &this->link);
    this->update(nullptr);
}


void SceneNode::raiseToTop() {
    SceneNode* currentTop = wl_container_of(
        parent->children.prev, currentTop, link
    );

    if (currentTop != this) {
        this->placeAbove(currentTop);
    }

}


void SceneNode::lowerToBottom() {
    SceneNode* currentBottom = wl_container_of(
        parent->children.next, currentBottom, link
    );

    if (this != currentBottom) {
        this->placeBelow(currentBottom);
    }

}


struct NodeAtData {
    double lx, ly;
    double rx, ry;
    SceneNode* node;
};


static bool sceneNodeAtOnDiscover(SceneNode* node, int lx, int ly, void* data) {
    auto* atData = (NodeAtData*) data;

    double rx = atData->lx - lx;
    double ry = atData->ly - ly;

    if (node->type() == SceneNodeType::BUFFER) {
        auto* buf = (SceneBufferNode*) node;

        if (buf->pointAcceptsInput && !buf->pointAcceptsInput(buf, &rx, &ry)) {
            return false;
        }
    }

    atData->rx = rx;
    atData->ry = ry;
    atData->node = node;
    return true;
}


SceneNode* SceneNode::nodeAt(double lx, double ly, double* nx, double* ny) {
    wlr_box box = {
        .x = (int) floor(lx),
        .y = (int) floor(ly),
        .width = 1,
        .height = 1
    };

    NodeAtData data = {
        .lx = lx,
        .ly = ly
    };

    if (this->nodesInBox(&box, sceneNodeAtOnDiscover, &data)) {
        if (nx) {
            *nx = data.rx;
        }

        if (ny) {
            *ny = data.ry;
        }

        return data.node;
    }

    return nullptr;
}


/* ------------ SceneNode 公共方法 结束 ------------ */


/* ------------ SceneTreeNode 开始 ------------ */

SceneTreeNode* SceneTreeNode::create(SceneTreeNode* parent) { 
    SceneTreeNode* p = new (std::nothrow) SceneTreeNode;

    if (p && p->init(parent)) {
        delete p;
        return nullptr;
    }
    
    return p;
}

int SceneTreeNode::init(SceneTreeNode* parent) {
    wl_list_init(&children);
    return reinterpret_cast<SceneNode*>(this)->init(parent);
}

SceneTreeNode::~SceneTreeNode() {
    
    this->setEnabled(false);
    Scene* scene = this->getRootScene();

    if (this == scene->tree) {
        Output *output, *tmp;
        wl_list_for_each_safe(output, tmp, &scene->outputs, link) {
            delete output;
        }

        wl_list_remove(&scene->eventListeners.linuxDmaBufV1Destroy.link);
    }

    SceneNode *child, *tmp;

    wl_list_for_each_safe(child, tmp, &this->children, link) {
        delete child;
    }
    
}

/* ------------ SceneTreeNode 结束 ------------ */


/* ------------ SceneBufferNode 开始 ------------ */

static void bufferReleaseEventBridge(wl_listener* listener, void* data);


static void sceneBufferNodeSetBuffer(SceneBufferNode* buf, wlr_buffer* wlrBuffer) {
    wl_list_remove(&buf->eventListeners.bufferRelease.link);
    wl_list_init(&buf->eventListeners.bufferRelease.link);
    if (buf->ownBuffer) {
        wlr_buffer_unlock(buf->wlrBuffer);
    }

    buf->wlrBuffer = nullptr;
    buf->ownBuffer = false;
    buf->bufferWidth = buf->bufferHeight = 0;
    buf->bufferIsOpaque = false;

    if (!wlrBuffer) {
        return;
    }

    buf->ownBuffer = true;
    buf->wlrBuffer = wlr_buffer_lock(wlrBuffer);
    buf->bufferWidth = wlrBuffer->width;
    buf->bufferHeight = wlrBuffer->height;
    buf->bufferIsOpaque = true; // todo

    buf->eventListeners.bufferRelease.notify = bufferReleaseEventBridge;
    wl_signal_add(&wlrBuffer->events.release, &buf->eventListeners.bufferRelease);
}


static void bufferReleaseEventBridge(wl_listener* listener, void* data) {
    SceneBufferNode* buf = wl_container_of(listener, buf, eventListeners.bufferRelease);
    sceneBufferNodeSetBuffer(buf, nullptr);
}


SceneBufferNode* SceneBufferNode::create(
    SceneTreeNode* parent, wlr_buffer* wlrBuffer
) {
    auto* p = new (nothrow) SceneBufferNode;
    if (!p) {
        return nullptr;
    }

    if (p->init(parent, wlrBuffer)) {
        delete p;
        return nullptr;
    }

    return p;
}

int SceneBufferNode::init(
    SceneTreeNode* parent, wlr_buffer* wlrBuffer
) {
    SceneNode* base = this;
    base->init(parent);

    wl_signal_init(&events.outputsUpdate);
    wl_signal_init(&events.outputEnter);
    wl_signal_init(&events.outputLeave);
    wl_signal_init(&events.outputSample);
    wl_signal_init(&events.frameDone);
    wl_list_init(&eventListeners.bufferRelease.link);

    opacity = 1;

    
    sceneBufferNodeSetBuffer(this, wlrBuffer);
    this->update(nullptr);

    return 0;
}


bool SceneBufferNode::invisible() {
    return wlrBuffer == nullptr && texture == nullptr;
}


SceneBufferNode::~SceneBufferNode() {
    
    this->setEnabled(false);
    Scene* scene = this->getRootScene();

    uint64_t active = this->activeOutputs;
    if (active) {
        Output* output;
        wl_list_for_each(output, &scene->outputs, link) {
            if (active & (1ull << output->index)) {
                wl_signal_emit_mutable(&this->events.outputLeave, output);
            }
        }

        wlr_texture_destroy(this->texture);
        sceneBufferNodeSetBuffer(this, nullptr);
    }
    
}

void SceneBufferNode::updateNodeUpdateOutputs(
    wl_list* outputs, Output* ignore, Output* force
) {
    uint32_t largestOverlap = 0;

    Output* oldPrimaryOutput = this->primaryOutput;
    this->primaryOutput = nullptr;

    size_t count = 0;
    uint64_t activeOutputs = 0;

    Output* sceneOutput;
    wl_list_for_each(sceneOutput, outputs, link) {
        if (sceneOutput == ignore) {
            return;
        }
        if (!sceneOutput->wlrOutput->enabled) {
            return;
        }

        wlr_box outputBox = {
            .x = sceneOutput->position.x,
            .y = sceneOutput->position.y
        };

        wlr_output_effective_resolution(
            sceneOutput->wlrOutput, &outputBox.width, &outputBox.height
        );

        pixman::Region32 intersection;
        intersection.intersectRect(visibleArea, outputBox);

        if (intersection.notEmpty()) {
            auto overlap = intersection.regionArea();
            if (overlap >= largestOverlap) {
                largestOverlap = overlap;
                primaryOutput = sceneOutput;
            }

            activeOutputs |= 1ull << sceneOutput->index;
            count++;
        }
    }

    if (oldPrimaryOutput != primaryOutput) {
        this->dmaBufPrevFeedbackOptions = {
            .main_renderer = nullptr,
            .scanout_primary_output = nullptr,
            .output_layer_feedback_event = nullptr,
        };
    }

    uint64_t oldActive = this->activeOutputs;
    this->activeOutputs = activeOutputs;

    wl_list_for_each(sceneOutput, outputs, link) {
        uint64_t mask = 1ull << sceneOutput->index;
        bool intersects = !!(activeOutputs & mask);
        bool intersectsBefore = !!(oldActive & mask);

        if (intersects && !intersectsBefore) {
            wl_signal_emit_mutable(&events.outputEnter, sceneOutput);
        } else if (!intersects && intersectsBefore) {
            wl_signal_emit_mutable(&events.outputLeave, sceneOutput);
        }
    }

    
    // 如果没有需要更新的内容，就不必发出 update 信号。

    bool canSkip = oldActive == activeOutputs 
        && (!force || ((1ull << force->index) & ~activeOutputs))
        && oldPrimaryOutput == primaryOutput;
    
    if (canSkip) {
        return;
    }

    Output* outputsArray[64];
    OutputsUpdateEvent event = {
        .active = outputsArray,
        .size = count
    };

    size_t i = 0;
    wl_list_for_each(sceneOutput, outputs, link) {
        if (activeOutputs & (1ull << sceneOutput->index)) {
            outputsArray[i++] = sceneOutput;
        }
    }

    wl_signal_emit_mutable(&events.outputsUpdate, &event);
}


// todo: 本函数在 Output.cpp 内重复定义。
static void scaleOutputDamage(pixman_region32_t* damage, float scale) {
    wlr_region_scale(damage, damage, scale);
    if (floor(scale) != scale) {
        wlr_region_expand(damage, damage, 1);
    }
}


void SceneBufferNode::setBuffer(wlr_buffer* wlrBuffer, pixman_region32_t* damage) {
    bool update = false;
    wlr_texture_destroy(this->texture);
    this->texture = nullptr;

    if (wlrBuffer) {
        update = dstHeight == 0 && dstWidth == 0 
            && (bufferWidth != wlrBuffer->width || bufferHeight != wlrBuffer->height);
    } else {
        update = true;
    }

    sceneBufferNodeSetBuffer(this, wlrBuffer);

    if (update) {
        this->update(nullptr);
        return;    
    }

    int lx, ly;
    if (!this->coords(&lx, &ly)) {
        return;
    }

    pixman::Region32 fallbackDamage = (const wlr_box) {
        .x = 0, .y = 0, .width = wlrBuffer->width, .height = wlrBuffer->height
    };
    
    if (!damage) {
        damage = fallbackDamage.raw();
    }

    wlr_fbox box = srcBox;
    if (wlr_fbox_empty(&box)) {
        box.x = box.y = 0;
        box.width = wlrBuffer->width;
        box.height = wlrBuffer->height;
    }

    wlr_fbox_transform(&box, &box, transform, wlrBuffer->width, wlrBuffer->height);

    float scaleX, scaleY;
    if (dstWidth || dstHeight) {
        scaleX = dstWidth / box.width;
        scaleY = dstHeight / box.height;
    } else {
        scaleX = wlrBuffer->width / box.width;
        scaleY = wlrBuffer->height / box.height;
    }

    pixman::Region32 transDamage;
    wlr_region_transform(
        transDamage.raw(), damage, transform, wlrBuffer->width, wlrBuffer->height
    );

    transDamage.intersectRect(transDamage, box);
    pixman_region32_translate(transDamage.raw(), -box.x, -box.y);

    Scene* scene = this->getRootScene();
    Output* sceneOutput;
    wl_list_for_each(sceneOutput, &scene->outputs, link) {
        float outputScale = sceneOutput->wlrOutput->scale;
        float outputScaleX = outputScale * scaleX;
        float outputScaleY = outputScale * scaleY;

        pixman::Region32 outputDamage;
        wlr_region_scale_xy(outputDamage.raw(), transDamage.raw(), outputScaleX, outputScaleY);

        /*
            输出的单个像素，实际对应 bufferScaleX * bufferScaleY 个像素。

            要考虑 buffer 的 upscale 和 downscale
        */
        float bufferScaleX = 1.0f / outputScaleX;
        float bufferScaleY = 1.0f / outputScaleY;
        
        int distX = 0;
        if (floor(bufferScaleX) != bufferScaleX) {
            distX = (int) ceilf(outputScaleX / 2.0f);
        }

        int distY = 0;
        if (floor(bufferScaleY) != bufferScaleY) {
            distY = (int) ceilf(outputScaleY / 20.f);
        }

        wlr_region_expand(
            outputDamage.raw(), outputDamage.raw(), distX >= distY ? distX : distY
        );

        pixman::Region32 cullRegion = this->visibleArea;
        
        scaleOutputDamage(cullRegion.raw(), outputScale);
        pixman_region32_translate(cullRegion.raw(), -lx * outputScale, -ly * outputScale);
        outputDamage.intersectWith(cullRegion);

        pixman_region32_translate(
            outputDamage.raw(),
            (int) round((lx - sceneOutput->position.x) * outputScale),
            (int) round((ly - sceneOutput->position.y) * outputScale)
        );

        if (wlr_damage_ring_add(&sceneOutput->wlrDamageRing, outputDamage.raw())) {
            sceneOutput->scheduleFrame();
        }
    }

}


wlr_texture* SceneBufferNode::getTexture(wlr_renderer* wlrRenderer) {
    if (wlrBuffer == nullptr || this->texture != nullptr) {
        return this->texture;
    }

    wlr_client_buffer* clientBuffer = wlr_client_buffer_get(wlrBuffer);
    if (clientBuffer) {
        return clientBuffer->texture;
    }

    this->texture = wlr_texture_from_buffer(wlrRenderer, this->wlrBuffer);
    if (this->texture && this->ownBuffer) {
        this->ownBuffer = false;
        wlr_buffer_unlock(this->wlrBuffer);
    }

    return this->texture;
}


void SceneBufferNode::unmarkClientBuffer() {
    if (!this->wlrBuffer) {
        return;
    }

    wlr_client_buffer* clientBuf = wlr_client_buffer_get(this->wlrBuffer);
    if (!clientBuf) {
        return;
    }

    // asserts that clientBuf->n_ignore_locks > 0

    clientBuf->n_ignore_locks--; 
}


void SceneBufferNode::setOpaqueRegion(pixman_region32_t* opaque) {
    if (this->opaqueRegion == opaque) {
        return;
    }

    this->opaqueRegion = opaque;

    int x, y;
    if (!this->coords(&x, &y)) {
        return;
    }

    pixman::Region32 updateRegion;
    bounds(x, y, updateRegion.raw());
    this->getRootScene()->updateRegion(updateRegion);
}


void SceneBufferNode::setOpaqueRegion(vesper::bindings::pixman::Region32& opaque) {
    this->setOpaqueRegion(opaque.raw());   
}


void SceneBufferNode::setSourceBox(wlr_fbox* srcBox) {
    if (wlr_fbox_equal(&this->srcBox, srcBox)) {
        return;
    }

    if (srcBox != nullptr) {
        this->srcBox = *srcBox;
    } else {
        this->srcBox.x = this->srcBox.y = this->srcBox.width = this->srcBox.height = 0;
    }

    this->update(nullptr);
}


void SceneBufferNode::setDstSize(int width, int height) {
    if (this->dstHeight == height && this->dstWidth == width) {
        return;
    }

    this->dstWidth = width;
    this->dstHeight = height;
    this->update(nullptr);
}


void SceneBufferNode::setTransform(wl_output_transform transform) {
    if (this->transform == transform) {
        return;
    }

    this->transform = transform;
    this->update(nullptr);
}



/* ------------ SceneBufferNode 结束 ------------ */


/* ------------ SceneRectNode 开始 ------------ */

SceneRectNode* SceneRectNode::create(
    SceneTreeNode* parent, int width, int height, const float color[4]
) {
    SceneRectNode* p = new (nothrow) SceneRectNode;
    if (!p) {
        return nullptr;
    }

    if (p->init(parent, width, height, color)) {
        delete p;
        return nullptr;
    }

    return p;
}

int SceneRectNode::init(
    SceneTreeNode* parent, int width, int height, const float color[4]
) {
    SceneNode* base = this;
    base->init(parent);

    this->width = width;
    this->height = height;

    memcpy(&this->color, color, sizeof(this->color));

    this->update(nullptr);

    return 0;
}

SceneRectNode::~SceneRectNode() {
    this->setEnabled(false);
}

bool SceneRectNode::invisible() {
    return color[3] == 0.f;
}

/* ------------ SceneRectNode 结束 ------------ */

}
