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
#include "./Surface.h"
#include "../../bindings/pixman/Region32.h"

#include <functional>

using namespace std;
using namespace vesper::bindings;

namespace vesper::desktop::scene {


static void linuxDmaBufV1DestroyEventBridge(wl_listener* listener, void* data) {
    Scene* scene = wl_container_of(listener, scene, eventListeners.linuxDmaBufV1Destroy);
    wl_list_remove(&scene->eventListeners.linuxDmaBufV1Destroy.link);
    wl_list_init(&scene->eventListeners.linuxDmaBufV1Destroy.link);
    scene->linuxDmaBufV1 = nullptr;
}


Scene* Scene::create() {
    Scene* scene = new (nothrow) Scene;

    if (scene && scene->init()) {
        delete scene;
        return nullptr;
    }

    return scene;
}

Scene::~Scene() {
    this->destroyTree();
}

int Scene::init() {

    tree = SceneTreeNode::create(nullptr);
    if (!tree) {
        LOG_ERROR("failed to create scene tree node!");
        return -1;
    }

    tree->rootScene = this;

    wl_list_init(&outputs);
    wl_list_init(&eventListeners.linuxDmaBufV1Destroy.link);

    return 0;
}


OutputLayout* Scene::attachWlrOutputLayout(wlr_output_layout* wlrOutputLayout) {
    auto* outputLayout = OutputLayout::create({
        .scene = this,
        .wlrOutputLayout = wlrOutputLayout
    });

    if (!outputLayout) {
        LOG_ERROR("failed to create output layout!");
    }

    return outputLayout;
}

void Scene::destroyTree() {
    if (tree) {
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

    node->visibleArea -= updateData->updateRegion;
    node->visibleArea += updateData->visible;

    node->visibleArea.intersectRect(node->visibleArea, box);

    if (updateData->calculateVisibility) {
        pixman::Region32 opaque;
        node->opaqueRegion(x, y, opaque.raw());
        pixman_region32_subtract(
            updateData->visible, updateData->visible, opaque.raw()
        );
    }

    if (node->type() == SceneNodeType::BUFFER) {
        ((SceneBufferNode*) node)->updateNodeUpdateOutputs(updateData->outputs, nullptr, nullptr);
    }

    return false; // 返回 false 以保证所有子节点都被遍历到。
}


void Scene::updateRegion(pixman::Region32& updateRegion) {
    this->updateRegion(updateRegion.raw());
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


void Scene::setLinuxDmaBufV1(wlr_linux_dmabuf_v1* linuxDmaBufV1) {
    if (this->linuxDmaBufV1 != nullptr) {
        LOG_ERROR("scene's current dmabuf is NOT nullptr!");
        return;
    }

    this->linuxDmaBufV1 = linuxDmaBufV1;
    eventListeners.linuxDmaBufV1Destroy.notify = linuxDmaBufV1DestroyEventBridge;
    wl_signal_add(&linuxDmaBufV1->events.destroy, &eventListeners.linuxDmaBufV1Destroy);
}


void Scene::sceneBufferSendDmaBufFeedback(
    SceneBufferNode* sceneBuffer,
    wlr_linux_dmabuf_feedback_v1_init_options* options
) {
    if (this->linuxDmaBufV1 == nullptr) {
        return;
    }

    Surface* surface = Surface::tryFindSurfaceFromBuffer(sceneBuffer);
    if (!surface) {
        return;
    }

    if (memcmp(options, &sceneBuffer->dmaBufPrevFeedbackOptions, sizeof(*options)) == 0) {
        return;
    }

    sceneBuffer->dmaBufPrevFeedbackOptions = *options;

    wlr_linux_dmabuf_feedback_v1 feedback;
    if (!wlr_linux_dmabuf_feedback_v1_init_with_options(&feedback, options)) {
        return;
    }

    wlr_linux_dmabuf_v1_set_surface_feedback(linuxDmaBufV1, surface->wlrSurface, &feedback);

    wlr_linux_dmabuf_feedback_v1_finish(&feedback);
}


}
