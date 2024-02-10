// SPDX-License-Identifier: MulanPSL-2.0

/*
 * XDG Shell
 * 
 * 创建于 2024年2月6日 北京市海淀区
 */

#include "./XdgShell.h"
#include "./Scene.h"
#include "./SceneNode.h"
#include "./Surface.h"

using namespace std;

namespace vesper::desktop::scene {


static void treeDestroyEventBridge(wl_listener* listener, void* data) {
    XdgSurface* sceneXdgSurface = wl_container_of(
        listener, sceneXdgSurface, eventListeners.treeDestroy
    );

    wl_list_remove(&sceneXdgSurface->eventListeners.treeDestroy.link);
    wl_list_remove(&sceneXdgSurface->eventListeners.xdgSurfaceDestroy.link);
    wl_list_remove(&sceneXdgSurface->eventListeners.xdgSurfaceMap.link);
    wl_list_remove(&sceneXdgSurface->eventListeners.xdgSurfaceUnmap.link);
    wl_list_remove(&sceneXdgSurface->eventListeners.xdgSurfaceCommit.link);

    delete sceneXdgSurface;
}


static void xdgSurfaceDestroyEventBridge(wl_listener* listener, void* data) {
    XdgSurface* sceneXdgSurface = wl_container_of(
        listener, sceneXdgSurface, eventListeners.xdgSurfaceDestroy
    );

    delete sceneXdgSurface->tree;
}


static void xdgSurfaceMapEventBridge(wl_listener* listener, void* data) {
    XdgSurface* sceneXdgSurface = wl_container_of(
        listener, sceneXdgSurface, eventListeners.xdgSurfaceMap
    );

    sceneXdgSurface->tree->setEnabled(true);
}


static void xdgSurfaceUnmapEventBridge(wl_listener* listener, void* data) {
    XdgSurface* sceneXdgSurface = wl_container_of(
        listener, sceneXdgSurface, eventListeners.xdgSurfaceUnmap
    );

    sceneXdgSurface->tree->setEnabled(false);
}


static void xdgSurfaceCommitEventBridge(wl_listener* listener, void* data) {
    XdgSurface* sceneXdgSurface = wl_container_of(
        listener, sceneXdgSurface, eventListeners.xdgSurfaceCommit
    );

    sceneXdgSurface->updatePosition();
}


VESPER_OBJ_UTILS_IMPL_CREATE(XdgSurface, XdgSurface::CreateOptions)

int XdgSurface::init(const CreateOptions& options) {
    this->wlrXdgSurface = options.wlrXdgSurface;

    this->tree = SceneTreeNode::create(options.parent);
    if (this->tree == nullptr) {
        LOG_ERROR("failed to create SceneTreeNode!");
        return -1;
    }

    auto* subSurfaceTree = SubSurfaceTree::create(this->tree, wlrXdgSurface->surface);
    if (subSurfaceTree == nullptr) {
        delete this->tree;
        return -1;
    }

    this->surfaceTree = subSurfaceTree->tree;
    
    this->eventListeners.treeDestroy.notify = treeDestroyEventBridge;
    wl_signal_add(&tree->basicEvents.destroy, &eventListeners.treeDestroy);

    this->eventListeners.xdgSurfaceDestroy.notify = xdgSurfaceDestroyEventBridge;
    wl_signal_add(&wlrXdgSurface->events.destroy, &eventListeners.xdgSurfaceDestroy);

    this->eventListeners.xdgSurfaceMap.notify = xdgSurfaceMapEventBridge;
    wl_signal_add(&wlrXdgSurface->surface->events.map, &eventListeners.xdgSurfaceMap);

    this->eventListeners.xdgSurfaceUnmap.notify = xdgSurfaceUnmapEventBridge;
    wl_signal_add(&wlrXdgSurface->surface->events.unmap, &eventListeners.xdgSurfaceUnmap);

    this->eventListeners.xdgSurfaceCommit.notify = xdgSurfaceCommitEventBridge;
    wl_signal_add(&wlrXdgSurface->surface->events.commit, &eventListeners.xdgSurfaceCommit);

    this->tree->setEnabled(wlrXdgSurface->surface->mapped);
    this->updatePosition();

    return 0;
}

XdgSurface::~XdgSurface() {

}


void XdgSurface::updatePosition() {
    wlr_box geo = {
        .x = 0,
        .y = 0,
        .width = 0,
        .height = 0,
    };

    wlr_xdg_surface_get_geometry(wlrXdgSurface, &geo);
    this->surfaceTree->setPosition(-geo.x, -geo.y);

    if (wlrXdgSurface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        wlr_xdg_popup* popup = wlrXdgSurface->popup;
        if (popup) {
            this->tree->setPosition(popup->current.geometry.x, popup->current.geometry.y);
        }
    }
}


} // namespace vesper::desktop::scene

