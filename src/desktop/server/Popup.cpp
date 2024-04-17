// SPDX-License-Identifier: MulanPSL-2.0

/*
 * XDG Popup
 * 
 * 创建于 2024年4月15日 上海市嘉定区安亭镇
 */


#include "../../log/Log.h"
#include "./Popup.h"
#include "./Server.h"
#include "../scene/Scene.h"
#include "../scene/XdgShell.h"

using namespace vesper::desktop;
using namespace std;

namespace vesper::desktop::server {

static void xdgPopupCommitEventBridge(wl_listener* listener, void* data) {
    Popup* popup = wl_container_of(listener, popup, eventListeners.commit);
    if (popup->wlrXdgPopup->base->initial_commit) {
        wlr_xdg_surface_schedule_configure(popup->wlrXdgPopup->base);
    }
}


static void xdgPopupDestroyEventBridge(wl_listener* listener, void* data) {
    Popup* popup = wl_container_of(listener, popup, eventListeners.destroy);
    
    delete popup;
}


Popup::~Popup() {
    wl_list_remove(&eventListeners.commit.link);
    wl_list_remove(&eventListeners.destroy.link);
}


VESPER_OBJ_UTILS_IMPL_CREATE(Popup, Popup::CreateOptions)

int Popup::init(const CreateOptions& options) {
    this->server = options.server;
    this->wlrXdgPopup = options.xdgPopup;

    wlr_xdg_surface* parent = wlr_xdg_surface_try_from_wlr_surface(wlrXdgPopup->parent);

    if (!parent) {
        LOG_ERROR("no parent for popup!");
        return -2;
    }

    auto* parentTree = (scene::SceneTreeNode*) parent->data;

    auto* sceneXdgSurface = scene::XdgSurface::create({
        .parent = parentTree,
        .wlrXdgSurface = wlrXdgPopup->base
    });
    
    if (sceneXdgSurface == nullptr) {
        LOG_ERROR("failed to create xdg surface!");
        return -1;
    }

    wlrXdgPopup->base->data = sceneXdgSurface->tree;

    this->eventListeners.commit.notify = xdgPopupCommitEventBridge;
    wl_signal_add(&wlrXdgPopup->base->surface->events.commit, &eventListeners.commit);

    this->eventListeners.destroy.notify = xdgPopupDestroyEventBridge;
    wl_signal_add(&wlrXdgPopup->events.destroy, &eventListeners.destroy);

    return 0;
}


}

