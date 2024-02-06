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
#include "../../bindings/pixman/Region32.h"

#include <functional>

using namespace std;
using namespace vesper::bindings;

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

    tree = SceneTreeNode::create(nullptr);
    if (!tree) {
        LOG_ERROR("failed to create scene tree node!");
        return -1;
    }

    tree->rootScene = this;

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


struct SceneUpdateData {
    pixman_region32_t* visible;
    pixman_region32_t* updateRegion;
    wl_list* outputs;
    bool calculateVisibility;
};


static bool sceneUpdateRegionNodeUpdateOnDiscover(
    SceneNode* node, int x, int y, void* data
) {
    auto* updateData = (SceneUpdateData*) data;
    wlr_box box = { .x = x, .y = y };
    node->getSize(&box.width, &box.height);

    pixman_region32_subtract(
        &node->visibleArea, &node->visibleArea, updateData->updateRegion
    );
    
    pixman_region32_union(
        &node->visibleArea, &node->visibleArea, updateData->visible
    );

    pixman_region32_intersect_rect(
        &node->visibleArea, &node->visibleArea, x, y, box.width, box.height
    );

    if (updateData->calculateVisibility) {
        pixman::Region32 opaque;
        node->opaqueRegion(x, y, opaque.raw());
        pixman_region32_subtract(
            updateData->visible, updateData->visible, opaque.raw()
        );
    }

    node->updateNodeUpdateOutputs(updateData->outputs, nullptr, nullptr);

    return false; // 确保遍历完所有 rect 和 buffer 节点。
}


void Scene::updateRegion(pixman_region32_t* updateRegion) {

    pixman::Region32 visible;
    pixman_region32_copy(visible.raw(), updateRegion);

    SceneUpdateData data = {
        .visible = visible.raw(),
        .updateRegion = updateRegion,
        .outputs = &outputs,
        .calculateVisibility = calculateVisibility
    };

    pixman_box32_t* regionBox = pixman_region32_extents(updateRegion);
    wlr_box box = {
        .x = regionBox->x1,
        .y = regionBox->y1,
        .width = regionBox->x2 - regionBox->x1,
        .height = regionBox->y2 - regionBox->y1
    };

    this->tree->nodesInBox(
        &box, sceneUpdateRegionNodeUpdateOnDiscover, &data
    );

}

void Scene::damageOutputs(pixman_region32_t* damage) {
    if (pixman_region32_empty(damage)) {
        return;
    }

    Output* output;
    wl_list_for_each(output, &outputs, link) {
        pixman_region32_t outputDamage;
        pixman_region32_init(&outputDamage);

        pixman_region32_copy(&outputDamage, damage);
        pixman_region32_translate(&outputDamage, -output->position.x, -output->position.y);

        if (wlr_damage_ring_add(&output->wlrDamageRing, &outputDamage)) {
            wlr_output_schedule_frame(output->wlrOutput);
        }

        pixman_region32_fini(&outputDamage);
    }

}


Output* Scene::getSceneOutput(wlr_output* output) {
    wlr_addon* addon = wlr_addon_find(&output->addons, this, &sceneOutputAddonImpl);
    if (addon == nullptr) {
        return nullptr;
    }

    Output* sceneOutput = wl_container_of(addon, sceneOutput, addon);
    return sceneOutput;
}


}
