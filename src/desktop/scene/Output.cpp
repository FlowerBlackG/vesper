// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Scene Output
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#include "./Output.h"

using namespace std;

namespace vesper::desktop::scene {

void Output::setPosition(int x, int y) {
    if (this->position.x == x && this->position.y == y) {
        return;
    }

    this->position.x = x;
    this->position.y = y;

    updateGeometry();
    
}

void Output::updateGeometry() {
    wlr_damage_ring_add_whole(&wlrDamageRing);
    wlr_output_schedule_frame(wlrOutput);

    // todo: scene node output update
}

void Output::destroy() {

    wl_signal_emit_mutable(&events.destroy, nullptr);

    // todo: scene node output update



    // todo

    wlr_damage_ring_finish(&wlrDamageRing);
    
    // todo

    wl_list_remove(&link);
    wl_list_remove(&eventListeners.outputCommit.link);
    wl_list_remove(&eventListeners.outputDamage.link);
    wl_list_remove(&eventListeners.outputNeedsFrame.link);
}

}
