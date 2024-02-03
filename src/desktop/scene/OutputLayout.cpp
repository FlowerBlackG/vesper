// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 显示屏布局
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#include "./OutputLayout.h"
#include "./Output.h"
#include "./SceneNode.h"
#include "./Scene.h"

using namespace std;

namespace vesper::desktop::scene {

/* ------------ OutputLayout 开始 ------------ */

static void outputLayoutLayoutDestroyEventBridge(wl_listener* listener, void* data) {
    OutputLayout* obj = wl_container_of(listener, obj, eventListeners.layoutDestroy);
    obj->destroy();
    delete obj;
}

static void outputLayoutSceneDestroyEventBridge(wl_listener* listener, void* data) {
    OutputLayout* obj = wl_container_of(listener, obj, eventListeners.sceneDestroy);
    obj->destroy();
    delete obj;
}

static void outputLayoutLayoutChangeEventBridge(wl_listener* listener, void* data) {
    OutputLayout* obj = wl_container_of(listener, obj, eventListeners.layoutChange);
    
    OutputLayoutOutput* it;
    
    wl_list_for_each(it, &obj->outputs, link) {
        it->sceneOutput->setPosition(
            it->wlrLayoutOutput->x, it->wlrLayoutOutput->y
        );
    }
}

int OutputLayout::init(Scene* scene, wlr_output_layout* wlrOutputLayout) {
    this->scene = scene;
    this->wlrLayout = wlrOutputLayout;

    wl_list_init(&outputs);

    eventListeners.layoutDestroy.notify = outputLayoutLayoutDestroyEventBridge;
    wl_signal_add(&wlrOutputLayout->events.destroy, &eventListeners.layoutDestroy);

    eventListeners.layoutChange.notify = outputLayoutLayoutChangeEventBridge;
    wl_signal_add(&wlrOutputLayout->events.change, &eventListeners.layoutChange);
    
    eventListeners.sceneDestroy.notify = outputLayoutSceneDestroyEventBridge;
    wl_signal_add(&scene->tree->events.destroy, &eventListeners.sceneDestroy);

    return 0;

}

void OutputLayout::destroy() {
    OutputLayoutOutput *obj, *tmp;
    wl_list_for_each_safe(obj, tmp, &outputs, link) {
        obj->destroy();
        delete obj;
    }

    wl_list_remove(&eventListeners.layoutChange.link);
    wl_list_remove(&eventListeners.layoutDestroy.link);
    wl_list_remove(&eventListeners.sceneDestroy.link);
}


/* ------------ OutputLayout 结束 ------------ */


/* ------------ OutputLayoutOutput 开始 ------------ */

void OutputLayoutOutput::destroy() {
    wl_list_remove(&eventListeners.layoutOutputDestroy.link);
    wl_list_remove(&eventListeners.sceneOutputDestroy.link);
    wl_list_remove(&link);
}

/* ------------ OutputLayoutOutput 结束 ------------ */

}
