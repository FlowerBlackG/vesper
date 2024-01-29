// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Scene Output
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#pragma once

#include "../../utils/wlroots-cpp.h"

namespace vesper::desktop::scene {

class Scene;

class Output {

public:
    void setPosition(int x, int y);
    void updateGeometry();

public:
    void destroy();

public:
    wlr_output* wlrOutput;

    /**
     * 用于挂到 Scene 的 outputs 列表。
     */
    wl_list link;

    Scene* scene;

    wlr_damage_ring wlrDamageRing;

    struct {
        wl_signal destroy;
    } events;

    struct {
        wl_listener outputCommit;
        wl_listener outputDamage;
        wl_listener outputNeedsFrame;
    } eventListeners;


    struct {
        int x;
        int y;
    } position;

    // todo

};

}
