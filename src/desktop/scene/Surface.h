// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Surface 和 SubSurface
 * 
 * 创建于 2024年2月6日 北京市海淀区
 */

#pragma once

#include "../../log/Log.h"
#include "../../utils/wlroots-cpp.h"

namespace vesper::desktop::scene {

class SceneTreeNode;
class SceneBufferNode;


class Surface {
public:
    static Surface* create(SceneTreeNode* parent, wlr_surface* wlrSurface);
    int init(SceneTreeNode* parent, wlr_surface* wlrSurface);
    void reconfigure();

    void setClip(wlr_box* clip);

    static Surface* tryFindSurfaceFromBuffer(SceneBufferNode* buf);


public:
    SceneBufferNode* buffer;
    wlr_surface* wlrSurface;

    wlr_box clip;
    wlr_addon addon;

    struct {
        wl_listener outputsUpdate;
        wl_listener outputEnter;
        wl_listener outputLeave;
        wl_listener outputSample;
        wl_listener frameDone;
        wl_listener surfaceDestroy;
        wl_listener surfaceCommit;
    } eventListeners;


};



class SubSurfaceTree {
public:
    
    static SubSurfaceTree* create(SceneTreeNode* parent, wlr_surface* wlrSurface);

    int init(SceneTreeNode* parent, wlr_surface* wlrSurface);
    bool createSubSurface(wlr_subsurface* wlrSubSurface);

    void reconfigure();
    bool reconfigureClip();

    SubSurfaceTree* findChildSubSurfaceFromWlrSubSurface(wlr_subsurface* wlrSubSurface);

public:
    SceneTreeNode* tree = nullptr;
    wlr_surface* wlrSurface = nullptr;
    Surface* sceneSurface = nullptr;

    struct {
        wl_listener surfaceDestroy;
        wl_listener surfaceCommit;
        wl_listener surfaceMap;
        wl_listener surfaceUnmap;
        wl_listener surfaceNewSubSurface;

        wl_listener subsurfaceDestroy;    
    } eventListeners;

    /**
     * 
     * nullable.
     */
    SubSurfaceTree* parent = nullptr;
    
    wlr_addon sceneAddon;
    wlr_addon surfaceAddon;

    wlr_box clip;


};


} // namespace vesper::desktop::scene
