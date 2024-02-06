// SPDX-License-Identifier: MulanPSL-2.0

/*
 * XDG Shell
 * 
 * 创建于 2024年2月6日 北京市海淀区
 */

#pragma once

#include "../../log/Log.h"
#include "../../utils/wlroots-cpp.h"

namespace vesper::desktop::scene {

class SceneTreeNode;


class XdgSurface {
public:
    static XdgSurface* create(SceneTreeNode* parent, wlr_xdg_surface* wlrXdgSurface);
    int init(SceneTreeNode* parent, wlr_xdg_surface* wlrXdgSurface);

    void updatePosition();

public:
    SceneTreeNode* tree;
    SceneTreeNode* surfaceTree;

    wlr_xdg_surface* wlrXdgSurface;

    struct {
        wl_listener treeDestroy;
        wl_listener xdgSurfaceDestroy;
        wl_listener xdgSurfaceMap;
        wl_listener xdgSurfaceUnmap;
        wl_listener xdgSurfaceCommit;
    } eventListeners;

};



} // namespace vesper::desktop::scene

