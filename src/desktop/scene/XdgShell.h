// SPDX-License-Identifier: MulanPSL-2.0

/*
 * XDG Shell
 * 
 * 创建于 2024年2月6日 北京市海淀区
 */

#pragma once

#include "../../log/Log.h"
#include "../../utils/wlroots-cpp.h"
#include "../../utils/ObjUtils.h"

namespace vesper::desktop::scene {

class SceneTreeNode;


class XdgSurface {
public:

    struct CreateOptions {
        SceneTreeNode* parent;
        wlr_xdg_surface* wlrXdgSurface;
    };

    static XdgSurface* create(const CreateOptions&);
    
    void updatePosition();

    ~XdgSurface();

protected:
    XdgSurface() {}
    VESPER_OBJ_UTILS_DISABLE_COPY(XdgSurface)
    int init(const CreateOptions&);

public:
    SceneTreeNode* tree = nullptr;
    SceneTreeNode* surfaceTree = nullptr;

    wlr_xdg_surface* wlrXdgSurface = nullptr;

    struct {
        wl_listener treeDestroy;
        wl_listener xdgSurfaceDestroy;
        wl_listener xdgSurfaceMap;
        wl_listener xdgSurfaceUnmap;
        wl_listener xdgSurfaceCommit;
    } eventListeners;

};



} // namespace vesper::desktop::scene

