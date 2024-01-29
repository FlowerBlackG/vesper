// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 场景管理（窗口管理）
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#include "./Scene.h"
#include "../../log/Log.h"
#include "./SceneNode.h"
#include "./OutputLayout.h"
#include "./Output.h"

using namespace std;

namespace vesper::desktop::scene {

Scene* Scene::create() {
    Scene* scene = new (nothrow) Scene;
    if (!scene) {
        return nullptr;
    }

    if (scene->init()) {
        delete scene;
        return nullptr;
    }

    return scene;
}

int Scene::init() {

    tree = SceneTreeNode::create();
    if (!tree) {
        LOG_ERROR("failed to create scene tree node!");
        return -1;
    }

    wl_list_init(&outputs);

    return 0;
}


OutputLayout* Scene::attachWlrOutputLayout(wlr_output_layout* wlrOutputLayout) {
    auto* outputLayout = new (nothrow) OutputLayout;
    if (!outputLayout) {
        LOG_ERROR("failed to create output layout!");
        return nullptr;
    }

    if (outputLayout->init(this, wlrOutputLayout)) {
        delete outputLayout;
        LOG_ERROR("failed to init output layout!");
        return nullptr;
    }

    return outputLayout;
}

void Scene::destroyTree() {
    if (tree) {
        tree->destroy();
        delete tree;
        tree = nullptr;
    }
}

}
