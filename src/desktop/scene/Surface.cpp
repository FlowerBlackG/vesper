// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Surface
 * 
 * 创建于 2024年2月6日 北京市海淀区
 */


#include "./Surface.h"
#include "./Scene.h"
#include "./SceneNode.h"
#include "./Output.h"
#include "./XdgShell.h"

#include "../../bindings/pixman/Region32.h"

using namespace std;

using namespace vesper::bindings;

namespace vesper::desktop::scene {


/* ------------ Surface 开始 ------------ */


static void surfaceOutputsUpdateEventBridge (wl_listener* listener, void* data) {
    Surface* surface = wl_container_of(listener, surface, eventListeners.outputsUpdate);
    
    if (surface->buffer->primaryOutput) {
        double scale = 1; // todo
        // todo: wlr_fractional_scale_v1_notify_scale
        wlr_surface_set_preferred_buffer_scale(surface->wlrSurface, ceil(scale));
    }
}


static void surfaceOutputEnterEventBridge (wl_listener* listener, void* data) {
    Surface* surface = wl_container_of(listener, surface, eventListeners.outputEnter);
    auto* output = (Output*) data;

    wlr_surface_send_enter(surface->wlrSurface, output->wlrOutput);
}


static void surfaceOutputLeaveEventBridge (wl_listener* listener, void* data) {
    Surface* surface = wl_container_of(listener, surface, eventListeners.outputLeave);
    
    auto* output = (Output*) data;

    wlr_surface_send_leave(surface->wlrSurface, output->wlrOutput);
}


static void surfaceOutputSampleEventBridge (wl_listener* listener, void* data) {
    Surface* surface = wl_container_of(listener, surface, eventListeners.outputSample);
    
    auto* event = (OutputSampleEvent*) data;
    Output* output = event->output;

    if (surface->buffer->primaryOutput != output) {
        return;
    }

    wlr_presentation_surface_textured_on_output(surface->wlrSurface, output->wlrOutput);
}


static void surfaceFrameDoneEventBridge (wl_listener* listener, void* data) {
    Surface* surface = wl_container_of(listener, surface, eventListeners.frameDone);
    
    auto* now = (timespec*) data;

    wlr_surface_send_frame_done(surface->wlrSurface, now);
}


static void surfaceSurfaceDestroyEventBridge (wl_listener* listener, void* data) {
    Surface* surface = wl_container_of(listener, surface, eventListeners.surfaceDestroy);
    
    surface->buffer->destroy();
    delete surface->buffer;
    surface->buffer = nullptr;
}


static void surfaceSurfaceCommitEventBridge (wl_listener* listener, void* data) {
    Surface* surface = wl_container_of(listener, surface, eventListeners.surfaceCommit);
    
    SceneBufferNode* sceneBuffer = surface->buffer;
    surface->reconfigure();

    int lx, ly;
    bool enabled = sceneBuffer->coords(&lx, &ly);

    if (
        !wl_list_empty(&surface->wlrSurface->current.frame_callback_list)
        && surface->buffer->primaryOutput != nullptr && enabled
    ) {
        wlr_output_schedule_frame(surface->buffer->primaryOutput->wlrOutput);
    }
}




static void surfaceAddonDestroy(wlr_addon* addon) {
    Surface* surface = wl_container_of(addon, surface, addon);

    surface->buffer->unmarkClientBuffer();
    
    wlr_addon_finish(&surface->addon);

    wl_list_remove(&surface->eventListeners.outputsUpdate.link);
    wl_list_remove(&surface->eventListeners.outputEnter.link);
    wl_list_remove(&surface->eventListeners.outputLeave.link);
    wl_list_remove(&surface->eventListeners.outputSample.link);
    wl_list_remove(&surface->eventListeners.frameDone.link);
    wl_list_remove(&surface->eventListeners.surfaceDestroy.link);
    wl_list_remove(&surface->eventListeners.surfaceCommit.link);

    delete surface;
}


wlr_addon_interface sceneSurfaceAddonImpl = {
    .name = "vesper_scene_surface",
    .destroy = surfaceAddonDestroy
};


Surface* Surface::create(SceneTreeNode* parent, wlr_surface* wlrSurface) {
    auto* p = new (nothrow) Surface;
    if (p && p->init(parent, wlrSurface)) {
        delete p;
        p = nullptr;
    }

    return p;
}


int Surface::init(SceneTreeNode* parent, wlr_surface* wlrSurface) {
    this->buffer = SceneBufferNode::create(parent, nullptr);
    
    if (this->buffer == nullptr) {
        return -1;
    }

    this->wlrSurface = wlrSurface;

    // todo: scene_buffer->point_accepts_input = scene_buffer_point_accepts_input

    this->eventListeners.outputsUpdate.notify = surfaceOutputsUpdateEventBridge;
    wl_signal_add(&buffer->events.outputsUpdate, &eventListeners.outputsUpdate);

    this->eventListeners.outputEnter.notify = surfaceOutputEnterEventBridge;
    wl_signal_add(&buffer->events.outputEnter, &eventListeners.outputEnter);

    this->eventListeners.outputLeave.notify = surfaceOutputLeaveEventBridge;
    wl_signal_add(&buffer->events.outputLeave, &eventListeners.outputLeave);

    this->eventListeners.outputSample.notify = surfaceOutputSampleEventBridge;
    wl_signal_add(&buffer->events.outputSample, &eventListeners.outputSample);

    this->eventListeners.frameDone.notify = surfaceFrameDoneEventBridge;
    wl_signal_add(&buffer->events.frameDone, &eventListeners.frameDone);

    this->eventListeners.surfaceDestroy.notify = surfaceSurfaceDestroyEventBridge;
    wl_signal_add(&wlrSurface->events.destroy, &eventListeners.surfaceDestroy);

    this->eventListeners.surfaceCommit.notify = surfaceSurfaceCommitEventBridge;
    wl_signal_add(&wlrSurface->events.commit, &eventListeners.surfaceCommit);

    wlr_addon_init(&this->addon, &buffer->addons, this->buffer, &sceneSurfaceAddonImpl);

    this->reconfigure();

    return 0;
}

void Surface::reconfigure() {
    SceneBufferNode* sceneBuffer = this->buffer;
    wlr_surface* surface = this->wlrSurface;
    wlr_surface_state* state = &surface->current;

    wlr_fbox srcBox;
    wlr_surface_get_buffer_source_box(surface, &srcBox);

    pixman::Region32 opaque;
    pixman_region32_copy(opaque.raw(), &surface->opaque_region);

    int width = state->width;
    int height = state->height;

    if (!wlr_box_empty(&this->clip)) {
        wlr_box* clip = &this->clip;

        int bufferWidth = state->buffer_width;
        int bufferHeight = state->buffer_height;
        width = min(clip->width, width - clip->x);
        height = min(clip->height, height - clip->y);

        wlr_fbox_transform(&srcBox, &srcBox, state->transform, bufferWidth, bufferHeight);
        wlr_output_transform_coords(state->transform, &bufferWidth, &bufferHeight);

        srcBox.x += (double) (clip->x * bufferWidth) / state->width;
        srcBox.y += (double) (clip->y * bufferHeight) / state->height;
        srcBox.width *= (double) width / state->width;
        srcBox.height *= (double) height / state->height;

        wlr_fbox_transform(
            &srcBox, &srcBox, 
            wlr_output_transform_invert(state->transform),
            bufferWidth, bufferHeight
        );

        pixman_region32_translate(opaque.raw(), -clip->x, -clip->y);
        opaque.intersectRect(opaque, 0, 0, width, height);
    }

    if (width <= 0 || height <= 0) {
        sceneBuffer->setBuffer(nullptr);
        return;
    }

    sceneBuffer->setOpaqueRegion(opaque.raw());
    sceneBuffer->setSourceBox(&srcBox);
    sceneBuffer->setDstSize(width, height);
    sceneBuffer->setTransform(state->transform);

    sceneBuffer->unmarkClientBuffer();

    if (surface->buffer) {
        surface->buffer->n_ignore_locks++;
        sceneBuffer->setBuffer(&surface->buffer->base, &surface->buffer_damage);
    } else {
        sceneBuffer->setBuffer(nullptr);
    }

}


void Surface::setClip(wlr_box* clip) {
    if (wlr_box_equal(clip, &this->clip)) {
        return;
    }

    if (clip) {
        this->clip = *clip;
    } else {
        this->clip.x = this->clip.y = this->clip.width = this->clip.height = 0;
    }

    this->reconfigure();
}


Surface* Surface::tryFindSurfaceFromBuffer(SceneBufferNode* buf) {
    wlr_addon* addon = wlr_addon_find(&buf->addons, buf, &sceneSurfaceAddonImpl);

    if (addon == nullptr) {
        return nullptr;
    }

    Surface* surface = wl_container_of(addon, surface, addon);
    return surface;
}

/* ------------ Surface 结束 ------------ */


/* ------------ SubSurface 开始 ------------ */


static void subSurfaceTreeAddonDestroy(wlr_addon* addon) {
    SubSurfaceTree* subSurfaceTree = wl_container_of(addon, subSurfaceTree, sceneAddon);

    if (subSurfaceTree->parent) {
        wlr_addon_finish(&subSurfaceTree->surfaceAddon);
        wl_list_remove(&subSurfaceTree->eventListeners.subsurfaceDestroy.link);
    }

    wlr_addon_finish(&subSurfaceTree->sceneAddon);

    wl_list_remove(&subSurfaceTree->eventListeners.surfaceDestroy.link);
    wl_list_remove(&subSurfaceTree->eventListeners.surfaceCommit.link);
    wl_list_remove(&subSurfaceTree->eventListeners.surfaceMap.link);
    wl_list_remove(&subSurfaceTree->eventListeners.surfaceUnmap.link);
    wl_list_remove(&subSurfaceTree->eventListeners.surfaceNewSubSurface.link);

    delete subSurfaceTree;
}


wlr_addon_interface sceneSubSurfaceTreeSurfaceAddonImpl = {
    .name = "vesper_scene_subsurface_tree",
    .destroy = subSurfaceTreeAddonDestroy
};



static void subSurfaceTreeSurfaceDestroyEventBridge(wl_listener* listener, void* data) {
    SubSurfaceTree* tree = wl_container_of(listener, tree, eventListeners.surfaceDestroy);
    tree->tree->destroy();
    delete tree->tree;
    tree->tree = nullptr;
}


static void subSurfaceTreeSurfaceCommitEventBridge(wl_listener* listener, void* data) {
    SubSurfaceTree* tree = wl_container_of(listener, tree, eventListeners.surfaceCommit);
    tree->reconfigure();
}


static void subSurfaceTreeSurfaceMapEventBridge(wl_listener* listener, void* data) {
    SubSurfaceTree* tree = wl_container_of(listener, tree, eventListeners.surfaceMap);
    tree->tree->setEnabled(true);
}


static void subSurfaceTreeSurfaceUnmapEventBridge(wl_listener* listener, void* data) {
    SubSurfaceTree* tree = wl_container_of(listener, tree, eventListeners.surfaceUnmap);
    tree->tree->setEnabled(false);
}


static void subSurfaceTreeSurfaceNewSubSurfaceEventBridge(
    wl_listener* listener, void* data
) {
    SubSurfaceTree* tree = wl_container_of(
        listener, tree, eventListeners.surfaceNewSubSurface
    );
    
    auto* wlrSubsurface = (wlr_subsurface*) data;
    if (!tree->createSubSurface(wlrSubsurface)) {
        wl_resource_post_no_memory(wlrSubsurface->resource);
    }
}


static void subSurfaceTreeSubSurfaceDestroyEventBridge(wl_listener* listener, void* data) {
    SubSurfaceTree* tree = wl_container_of(
        listener, tree, eventListeners.subsurfaceDestroy
    );

    tree->tree->destroy();
    delete tree->tree;
    tree->tree = nullptr;
}


SubSurfaceTree* SubSurfaceTree::create(SceneTreeNode* parent, wlr_surface* wlrSurface) {
    auto* p = new (nothrow) SubSurfaceTree;
    if (p && p->init(parent, wlrSurface)) {
        delete p;
        p = nullptr;
    }

    return p;
}


int SubSurfaceTree::init(SceneTreeNode* parent, wlr_surface* wlrSurface) {

    this->clip = {
        .x = 0,
        .y = 0,
        .width = 0,
        .height = 0,
    };

    this->tree = SceneTreeNode::create(parent);
    if (this->tree == nullptr) {
        return -1;
    }

    this->sceneSurface = Surface::create(this->tree, wlrSurface);
    if (this->sceneSurface == nullptr) {
        this->tree->destroy();
        delete this->tree;
        return -2;
    }

    this->wlrSurface = wlrSurface;

    wlr_subsurface* wlrSubSurface;
    wl_list_for_each(
        wlrSubSurface, &wlrSurface->current.subsurfaces_below, current.link
    ) {
        if (!this->createSubSurface(wlrSubSurface)) {
            this->tree->destroy();
            delete this->tree;
            return -3;
        }
    }

    wl_list_for_each(
        wlrSubSurface, &wlrSurface->current.subsurfaces_above, current.link
    ) {
        if (!this->createSubSurface(wlrSubSurface)) {
            this->tree->destroy();
            delete this->tree;
            return -4;
        }
    }

    this->reconfigure();

    wlr_addon_init(
        &sceneAddon, &tree->addons, nullptr, &sceneSubSurfaceTreeSurfaceAddonImpl
    );


    this->eventListeners.surfaceDestroy.notify = subSurfaceTreeSurfaceDestroyEventBridge;
    wl_signal_add(&wlrSurface->events.destroy, &eventListeners.surfaceDestroy);

    this->eventListeners.surfaceCommit.notify = subSurfaceTreeSurfaceCommitEventBridge;
    wl_signal_add(&wlrSurface->events.commit, &eventListeners.surfaceCommit);

    this->eventListeners.surfaceMap.notify = subSurfaceTreeSurfaceMapEventBridge;
    wl_signal_add(&wlrSurface->events.map, &eventListeners.surfaceMap);

    this->eventListeners.surfaceUnmap.notify = subSurfaceTreeSurfaceUnmapEventBridge;
    wl_signal_add(&wlrSurface->events.unmap, &eventListeners.surfaceUnmap);

    this->eventListeners.surfaceNewSubSurface.notify = 
        subSurfaceTreeSurfaceNewSubSurfaceEventBridge;

    wl_signal_add(&wlrSurface->events.new_subsurface, &eventListeners.surfaceNewSubSurface);


    this->tree->setEnabled(wlrSurface->mapped);

    return 0;
}


bool SubSurfaceTree::createSubSurface(wlr_subsurface* wlrSubSurface) {
    SubSurfaceTree* child = SubSurfaceTree::create(this->tree, wlrSubSurface->surface);
    if (child == nullptr) {
        return false;
    }

    child->parent = this;

    wlr_addon_init(
        &child->surfaceAddon, 
        &wlrSubSurface->surface->addons, 
        this, 
        &sceneSubSurfaceTreeSurfaceAddonImpl
    );

    auto& destroyHandler = subSurfaceTreeSubSurfaceDestroyEventBridge;
    child->eventListeners.subsurfaceDestroy.notify = destroyHandler;
    wl_signal_add(&wlrSubSurface->events.destroy, &child->eventListeners.subsurfaceDestroy);

    return true;
}


void SubSurfaceTree::reconfigure() {
    bool hasClip = this->reconfigureClip();

    wlr_surface* surface = this->wlrSurface;

    SceneNode* prev = nullptr;
    wlr_subsurface* subsurface;
    wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
        SubSurfaceTree* child = this->findChildSubSurfaceFromWlrSubSurface(subsurface);
        if (prev) {
            child->tree->placeAbove(prev);
        }

        prev = child->tree;

        child->tree->setPosition(subsurface->current.x, subsurface->current.y);

        if (hasClip) {
            child->reconfigureClip();
        }
    }

    if (prev) {
        this->sceneSurface->buffer->placeAbove(prev);
    }

    prev = this->sceneSurface->buffer;

    wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
        SubSurfaceTree* child = this->findChildSubSurfaceFromWlrSubSurface(subsurface);
        child->tree->placeAbove(prev);
        prev = child->tree;

        child->tree->setPosition(subsurface->current.x, subsurface->current.y);

        if (hasClip) {
            child->reconfigureClip();
        }
    }

}


bool SubSurfaceTree::reconfigureClip() {
    if (this->parent) {
        this->clip = (wlr_box) {
            .x = this->parent->clip.x - tree->offset.x,
            .y = this->parent->clip.y - tree->offset.y,
            .width = parent->clip.width,
            .height = parent->clip.height
        };
    }

    if (wlr_box_empty(&this->clip)) {
        this->sceneSurface->setClip(nullptr);
        this->sceneSurface->buffer->setEnabled(true);
        this->sceneSurface->buffer->setPosition(0, 0);

        return false;
    } else {
        wlr_box clip = this->clip;
        wlr_box surfaceBox = {
            .width = this->wlrSurface->current.width,
            .height = this->wlrSurface->current.height
        };

        bool intersects = wlr_box_intersection(&clip, &clip, &surfaceBox);
        this->sceneSurface->buffer->setEnabled(intersects);

        if (intersects) {
            this->sceneSurface->buffer->setPosition(clip.x, clip.y);
            this->sceneSurface->setClip(&clip);
        }

        return true;
    }
}


SubSurfaceTree* SubSurfaceTree::findChildSubSurfaceFromWlrSubSurface(
    wlr_subsurface* wlrSubSurface
) {
    wlr_addon* addon = wlr_addon_find(
        &wlrSubSurface->surface->addons, this, &sceneSubSurfaceTreeSurfaceAddonImpl 
    );

    if (addon == nullptr) {
        LOG_ERROR("addon should not be null!");
        return nullptr;
    }

    SubSurfaceTree* subsurfaceTree = wl_container_of(addon, subsurfaceTree, surfaceAddon);

    return subsurfaceTree;
}

/* ------------ SubSurface 结束 ------------ */


} // namespace vesper::desktop::scene
