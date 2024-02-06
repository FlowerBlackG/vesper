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

#include "../../log/Log.h"

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
    wl_signal_add(&scene->tree->basicEvents.destroy, &eventListeners.sceneDestroy);

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


int OutputLayout::addOutput(
    wlr_output_layout_output* wlrLayoutOutput, Output* sceneOutput
) {
    OutputLayoutOutput* olo;
    wl_list_for_each(olo, &this->outputs, link) {
        if (olo->sceneOutput == sceneOutput) {
            LOG_ERROR("bad output!");
            return -1;
        }
    }

    olo = OutputLayoutOutput::create(sceneOutput, wlrLayoutOutput, this);
    if (!olo) {
        LOG_ERROR("failed to create scene output layout output!");
        return -1;
    }

    return 0;
}


/* ------------ OutputLayout 结束 ------------ */


/* ------------ OutputLayoutOutput 开始 ------------ */


static void layoutOutputDestroyEventBridge(wl_listener* listener, void* data) {
    OutputLayoutOutput* olo = wl_container_of(
        listener, olo, eventListeners.layoutOutputDestroy
    );

    olo->destroy();
    delete olo;
}


static void sceneOutputDestroyEventBridge(wl_listener* listener, void* data) {
    OutputLayoutOutput* olo = wl_container_of(
        listener, olo, eventListeners.sceneOutputDestroy
    );

    olo->destroy();
    delete olo;
}


OutputLayoutOutput* OutputLayoutOutput::create(
    Output* sceneOutput, wlr_output_layout_output* wlrLayoutOutput, OutputLayout* outputLayout
) {
    auto* p = new (nothrow) OutputLayoutOutput;
    if (!p) {
        return nullptr;
    }

    if (p->init(sceneOutput, wlrLayoutOutput, outputLayout)) {
        delete p;
        return nullptr;
    }

    return p;
}

int OutputLayoutOutput::init(
    Output* sceneOutput, wlr_output_layout_output* wlrLayoutOutput, OutputLayout* outputLayout
) {
    this->sceneOutput = sceneOutput;
    this->wlrLayoutOutput = wlrLayoutOutput;

    this->eventListeners.layoutOutputDestroy.notify = layoutOutputDestroyEventBridge;
    wl_signal_add(&wlrLayoutOutput->events.destroy, &eventListeners.layoutOutputDestroy);

    this->eventListeners.sceneOutputDestroy.notify = sceneOutputDestroyEventBridge;
    wl_signal_add(&sceneOutput->events.destroy, &eventListeners.sceneOutputDestroy);
    
    wl_list_insert(&outputLayout->outputs, &this->link);

    sceneOutput->setPosition(wlrLayoutOutput->x, wlrLayoutOutput->y);

    return 0;
}
    

void OutputLayoutOutput::destroy() {
    wl_list_remove(&eventListeners.layoutOutputDestroy.link);
    wl_list_remove(&eventListeners.sceneOutputDestroy.link);
    wl_list_remove(&link);
}

/* ------------ OutputLayoutOutput 结束 ------------ */

}
