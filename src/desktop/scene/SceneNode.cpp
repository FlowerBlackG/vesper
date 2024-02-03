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
    wl_signal_init(&events.destroy);
    wl_list_init(&link);
    pixman_region32_init(&visibleArea);

    if (parent) {
        wl_list_insert(parent->children.prev, &link); // 尾插。
    }
}


void SceneNode::destroy(std::function<void(Scene*)> typeDestroy) {
    wl_signal_emit_mutable(&events.destroy, nullptr);
    // todo: wlr_addon_set_finish(&node->addons);

    this->setEnabled(false);

    auto* scene = getRootScene();
    typeDestroy(scene);

    wl_list_remove(&link);
    pixman_region32_fini(&visibleArea);
}


Scene* SceneNode::getRootScene() {
    SceneNode* node = this;
    while (node->parent) {
        node = node->parent;
    }

    Scene* scene = wl_container_of(node, scene, tree);
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
    } else {
        updateNodeUpdateOutputs(outputs, ignore, force);
    }
}


void SceneNode::updateNodeUpdateOutputs(wl_list* outputs, Output* ignore, Output* force) {
    if (type() == SceneNodeType::BUFFER) {
        ((SceneBufferNode*) this)->updateNodeUpdateOutputs(outputs, ignore, force);
    } else {
        return;
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

    pixman_region32_t updateRegion;
    pixman_region32_init(&updateRegion);
    pixman_region32_copy(&updateRegion, damage);
    this->bounds(x, y, &updateRegion);

    scene->updateRegion(&updateRegion);
    pixman_region32_fini(&updateRegion);

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
        pixman_region32_union(visible, visible, &visibleArea);
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
        wl_list_for_each(child, &tree->children, link) {
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
            pixman_region32_copy(opaque, &buf->opaqueRegion);
            pixman_region32_intersect_rect(opaque, opaque, 0, 0, width, height);
            pixman_region32_translate(opaque, x, y);
            return;
        }
    }

    pixman_region32_fini(opaque);
    pixman_region32_init_rect(opaque, x, y, width, height);
}

/* ------------ SceneNode 公共方法 结束 ------------ */


/* ------------ SceneTreeNode 开始 ------------ */

SceneTreeNode* SceneTreeNode::create(SceneTreeNode* parent) { 
    SceneTreeNode* p = new (std::nothrow) SceneTreeNode;
    
    if (!p) { 
        return nullptr; 
    }

    if (p->init(parent)) {
        delete p;
        return nullptr;
    }
    
    return p;
}

int SceneTreeNode::init(SceneTreeNode* parent) {
    wl_list_init(&children);
    return reinterpret_cast<SceneNode*>(this)->init(parent);
}

void SceneTreeNode::destroy() {

    SceneNode* base = this;

    base->destroy( [&] (Scene* scene) {

        if (this == scene->tree) {
            Output *output, *tmp;
            wl_list_for_each_safe(output, tmp, &scene->outputs, link) {
                output->destroy();
                delete output;
            }
        }

        SceneNode *child, *tmp;

        wl_list_for_each_safe(child, tmp, &this->children, link) {

            if (child->type() == SceneNodeType::TREE) {
                ((SceneTreeNode*) child)->destroy();
            } else if (child->type() == SceneNodeType::BUFFER) {
                ((SceneBufferNode*) child)->destroy();
            } else if (child->type() == SceneNodeType::RECT) {
                ((SceneRectNode*) child)->destroy();
            } else {
                LOG_ERROR("bad type for scene node: ", child->type());
            }

            delete child;
            
        } 

    });

}

/* ------------ SceneTreeNode 结束 ------------ */


/* ------------ SceneBufferNode 开始 ------------ */

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
    pixman_region32_init(&opaqueRegion);
    wl_list_init(&eventListeners.bufferRelease.link);

    opacity = 1;

    this->setBuffer(wlrBuffer);

    this->update(nullptr);

    return 0;
}

void SceneBufferNode::destroy() {
    SceneNode* base = this;
    base->destroy([&] (Scene* scene) {
        uint64_t active = this->activeOutputs;
        if (active) {
            Output* output;
            wl_list_for_each(output, &scene->outputs, link) {
                if (active & (1ull << output->index)) {
                    wl_signal_emit_mutable(&this->events.outputLeave, output);
                }
            }

            wlr_texture_destroy(this->texture);
            this->setBuffer(nullptr);
            pixman_region32_fini(&this->opaqueRegion);
        }
    });
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
        intersection.intersectRect(&visibleArea, outputBox);

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
        // todo: linux dmabuf feedback init options
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


static void bufferReleaseEventBridge(wl_listener* listener, void* data) {
    SceneBufferNode* buf = wl_container_of(listener, buf, eventListeners.bufferRelease);
    buf->setBuffer(nullptr);
}


void SceneBufferNode::setBuffer(wlr_buffer* wlrBuffer) {
    wl_list_remove(&eventListeners.bufferRelease.link);
    wl_list_init(&eventListeners.bufferRelease.link);
    if (ownBuffer) {
        wlr_buffer_unlock(this->wlrBuffer);
    }

    this->wlrBuffer = nullptr;
    this->ownBuffer = false;
    this->bufferWidth = this->bufferHeight = 0;
    this->bufferIsOpaque = false;

    if (!wlrBuffer) {
        return;
    }

    this->ownBuffer = true;
    this->wlrBuffer = wlr_buffer_lock(wlrBuffer);
    this->bufferWidth = wlrBuffer->width;
    this->bufferHeight = wlrBuffer->height;
    this->bufferIsOpaque = false; // todo

    this->eventListeners.bufferRelease.notify = bufferReleaseEventBridge;
    wl_signal_add(&wlrBuffer->events.release, &eventListeners.bufferRelease);
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

void SceneRectNode::destroy() {
    SceneNode* base = this;
    base->destroy([] (Scene*) {});
}

/* ------------ SceneRectNode 结束 ------------ */

}
