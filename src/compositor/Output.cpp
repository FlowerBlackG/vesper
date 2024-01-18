// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Output
 * 
 * 创建于 2023年12月27日 上海市嘉定区安亭镇
 */


#include "./Output.h"
#include "../log/Log.h"

#include "./Compositor.h"

using namespace std;

namespace vesper::compositor {

static void frameEventBridge(wl_listener* listener, void* data) {
   
   Output* output = wl_container_of(listener, output, eventListeners.frame);

/////////////////////////////
static uint64_t count = 0;
LOG_TEMPORARY("frame tick: ", count++);
////////////////////////////
    output->frameEventHandler();
}


static void requestStateEventBridge(wl_listener* listener, void* data) {

    Output* output = wl_container_of(listener, output, eventListeners.frame);
    output->requestStateEventHandler();
}

static void destroyEventBridge(wl_listener* listener, void* data) {
    
    Output* output = wl_container_of(listener, output, eventListeners.frame);
    output->destroyEventHandler();
}

int Output::init(Compositor* compositor, wlr_output* output) {

    this->compositor = compositor;
    this->wlrOutput = output;

    eventListeners.frame.notify = frameEventBridge;
    wl_signal_add(&wlrOutput->events.frame, &eventListeners.frame);

    eventListeners.requestState.notify = requestStateEventBridge;
    wl_signal_add(&wlrOutput->events.request_state, &eventListeners.requestState);

    eventListeners.destroy.notify = destroyEventBridge;
    wl_signal_add(&wlrOutput->events.destroy, &eventListeners.destroy);

    wl_list_insert(&compositor->wlOutputs, &link);

    wlr_output_layout_output* layoutOutput = wlr_output_layout_add_auto(
        compositor->wlrOutputLayout, wlrOutput
    );
    wlr_scene_output* sceneOutput = wlr_scene_output_create(compositor->wlrScene, wlrOutput);
    wlr_scene_output_layout_add_output(compositor->wlrSceneLayout, layoutOutput, sceneOutput);

    LOG_INFO("compositor new output added.")

    return 0;
}


void Output::frameEventHandler() {
    wlr_scene* scene = compositor->wlrScene;

    wlr_scene_output* sceneOutput = wlr_scene_get_scene_output(scene, wlrOutput);

    wlr_scene_output_commit(sceneOutput, nullptr);

    wlr_renderer* renderer = compositor->wlrRenderer;
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(sceneOutput, &now);

}

void Output::requestStateEventHandler() {
    // todo
}

void Output::destroyEventHandler() {
    // todo
}


}