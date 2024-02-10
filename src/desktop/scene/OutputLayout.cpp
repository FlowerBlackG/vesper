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
    delete obj;
}

static void outputLayoutSceneDestroyEventBridge(wl_listener* listener, void* data) {
    OutputLayout* obj = wl_container_of(listener, obj, eventListeners.sceneDestroy);
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

VESPER_OBJ_UTILS_IMPL_CREATE(OutputLayout, OutputLayout::CreateOptions)

int OutputLayout::init(const CreateOptions& options) {
    this->scene = options.scene;
    this->wlrLayout = options.wlrOutputLayout;

    wl_list_init(&outputs);

    eventListeners.layoutDestroy.notify = outputLayoutLayoutDestroyEventBridge;
    wl_signal_add(&wlrLayout->events.destroy, &eventListeners.layoutDestroy);

    eventListeners.layoutChange.notify = outputLayoutLayoutChangeEventBridge;
    wl_signal_add(&wlrLayout->events.change, &eventListeners.layoutChange);
    
    eventListeners.sceneDestroy.notify = outputLayoutSceneDestroyEventBridge;
    wl_signal_add(&scene->tree->basicEvents.destroy, &eventListeners.sceneDestroy);

    return 0;

}

OutputLayout::~OutputLayout() {
    OutputLayoutOutput *obj, *tmp;
    wl_list_for_each_safe(obj, tmp, &outputs, link) {
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

    olo = OutputLayoutOutput::create({
        .sceneOutput = sceneOutput,
        .wlrLayoutOutput = wlrLayoutOutput,
        .outputLayout = this
    });

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

    delete olo;
}


static void sceneOutputDestroyEventBridge(wl_listener* listener, void* data) {
    OutputLayoutOutput* olo = wl_container_of(
        listener, olo, eventListeners.sceneOutputDestroy
    );

    delete olo;
}

VESPER_OBJ_UTILS_IMPL_CREATE(OutputLayoutOutput, OutputLayoutOutput::CreateOptions)

int OutputLayoutOutput::init(const CreateOptions& options) {
    this->sceneOutput = options.sceneOutput;
    this->wlrLayoutOutput = options.wlrLayoutOutput;

    this->eventListeners.layoutOutputDestroy.notify = layoutOutputDestroyEventBridge;
    wl_signal_add(&wlrLayoutOutput->events.destroy, &eventListeners.layoutOutputDestroy);

    this->eventListeners.sceneOutputDestroy.notify = sceneOutputDestroyEventBridge;
    wl_signal_add(&sceneOutput->events.destroy, &eventListeners.sceneOutputDestroy);
    
    wl_list_insert(&options.outputLayout->outputs, &this->link);

    sceneOutput->setPosition(wlrLayoutOutput->x, wlrLayoutOutput->y);

    return 0;
}
    

OutputLayoutOutput::~OutputLayoutOutput() {
    wl_list_remove(&eventListeners.layoutOutputDestroy.link);
    wl_list_remove(&eventListeners.sceneOutputDestroy.link);
    wl_list_remove(&link);
}

/* ------------ OutputLayoutOutput 结束 ------------ */

}
