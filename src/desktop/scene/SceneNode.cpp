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

using namespace std;

namespace vesper::desktop::scene {

/* ------------ SceneNode 公共方法 开始 ------------ */

int SceneNode::init(SceneNode* parent) {
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

/* ------------ SceneNode 公共方法 结束 ------------ */
/* ======================================================= */
/* ------------ SceneTreeNode 开始 ------------ */

int SceneTreeNode::init() {
    wl_list_init(&children);
    return reinterpret_cast<SceneNode*>(this)->init(nullptr);
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
/* ======================================================= */
/* ------------ SceneBufferNode 开始 ------------ */

int SceneBufferNode::init() {
    // todo
    return 0;
}

void SceneBufferNode::destroy() {
    SceneNode* base = this;
    base->destroy([&] (Scene* scene) {
        // todo
    });
}

/* ------------ SceneBufferNode 结束 ------------ */
/* ======================================================= */
/* ------------ SceneRectNode 开始 ------------ */

int SceneRectNode::init() {
    // todo
    return 0;
}

void SceneRectNode::destroy() {
    SceneNode* base = this;
    base->destroy([] (Scene*) {});
}

/* ------------ SceneRectNode 结束 ------------ */

}
