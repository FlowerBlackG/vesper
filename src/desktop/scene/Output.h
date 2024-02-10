// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Scene Output
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#pragma once

#include "../../utils/wlroots-cpp.h"

#include "../../bindings/pixman.h"

#include <vector>

namespace vesper::desktop::scene {

class Scene;
class SceneNode;
class RenderTimer;


class Output {

public:

    static Output* create(Scene* scene, wlr_output* wlrOutput);
    int init(Scene* scene, wlr_output* wlrOutput);

    void setPosition(int x, int y);
    void updateGeometry(bool forceUpdate);


    struct StateOptions {
        RenderTimer* timer;
    };


    bool commit(StateOptions* options);

    bool buildState(wlr_output_state* state, StateOptions* options);

    void sendFrameDone(timespec* now);

public:
    ~Output();

public:
    wlr_output* wlrOutput;

    /**
     * 用于挂到 Scene 的 outputs 列表。
     */
    wl_list link;

    Scene* scene;

    /**
     * 用于挂到 wlr_output 上。
     */
    wlr_addon addon;

    wlr_damage_ring wlrDamageRing;

    struct {
        wl_signal destroy;
    } events;

    struct {
        wl_listener outputCommit;
        wl_listener outputDamage;
        wl_listener outputNeedsFrame;
    } eventListeners;

    /** 屏幕（或虚拟屏幕）在所有屏幕组成的布局中的位置。 */
    struct {
        int x;
        int y;
    } position;

    vesper::bindings::pixman::Region32 pendingCommitDamage;

    uint8_t index;

    struct RenderListEntry {
        SceneNode* node;
        bool sentDmaBufFeedback;
        int x, y;
    };

    std::vector<RenderListEntry> renderList;

}; // class Output


extern wlr_addon_interface sceneOutputAddonImpl;


struct OutputSampleEvent {
    Output* output;    
};



} // namespace vesper::desktop::scene

