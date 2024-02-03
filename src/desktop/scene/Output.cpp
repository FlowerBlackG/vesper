// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Scene Output
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#include "./Output.h"
#include "./Scene.h"
#include "./SceneNode.h"

using namespace std;

namespace vesper::desktop::scene {

void Output::setPosition(int x, int y) {
    if (this->position.x == x && this->position.y == y) {
        return;
    }

    this->position.x = x;
    this->position.y = y;

    updateGeometry(false);
    
}

void Output::updateGeometry(bool forceUpdate) {
    wlr_damage_ring_add_whole(&wlrDamageRing);
    wlr_output_schedule_frame(wlrOutput);

    this->scene->tree->outputUpdate(
        &this->scene->outputs, 
        nullptr, 
        forceUpdate ? this : nullptr
    );
}

void Output::destroy() {

    wl_signal_emit_mutable(&events.destroy, nullptr);

    this->scene->tree->outputUpdate(&scene->outputs, this, nullptr);

    // todo: highlight region

    wlr_damage_ring_finish(&wlrDamageRing);
    
    pixman_region32_fini(&pendingCommitDamage);

    wl_list_remove(&link);
    wl_list_remove(&eventListeners.outputCommit.link);
    wl_list_remove(&eventListeners.outputDamage.link);
    wl_list_remove(&eventListeners.outputNeedsFrame.link);

    wl_array_release(&this->renderList);
}

}
