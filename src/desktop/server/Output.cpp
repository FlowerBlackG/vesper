// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Output
 * 
 * 创建于 2023年12月27日 上海市嘉定区安亭镇
 */


#include "./Output.h"
#include "../../log/Log.h"
#include "../scene/SceneNode.h"
#include "../scene/Scene.h"

#include "./Server.h"

using namespace std;

namespace vesper::desktop::server {

static void frameEventBridge(wl_listener* listener, void* data) {
   
    Output* output = wl_container_of(listener, output, eventListeners.frame);

    output->frameEventHandler();
}


static void requestStateEventBridge(wl_listener* listener, void* data) {

    Output* output = wl_container_of(listener, output, eventListeners.requestState);
    auto* event = (wlr_output_event_request_state*) data;
    wlr_output_commit_state(output->wlrOutput, event->state);
}

static void destroyEventBridge(wl_listener* listener, void* data) {
    
    Output* output = wl_container_of(listener, output, eventListeners.destroy);
    
    wl_list_remove(&output->eventListeners.destroy.link);
    wl_list_remove(&output->eventListeners.requestState.link);
    wl_list_remove(&output->eventListeners.frame.link);
    wl_list_remove(&output->link);

    auto* server = output->server; // todo: 目前仅支持一个输出目标。

    delete output;

    wl_display_terminate(server->wlDisplay);
}

int Output::init(Server* server, wlr_output* output) {

    this->server = server;
    this->wlrOutput = output;

    eventListeners.frame.notify = frameEventBridge;
    wl_signal_add(&wlrOutput->events.frame, &eventListeners.frame);

    eventListeners.requestState.notify = requestStateEventBridge;
    wl_signal_add(&wlrOutput->events.request_state, &eventListeners.requestState);

    eventListeners.destroy.notify = destroyEventBridge;
    wl_signal_add(&wlrOutput->events.destroy, &eventListeners.destroy);

    wl_list_insert(&server->wlOutputs, &link);

    wlr_output_layout_output* layoutOutput = wlr_output_layout_add_auto(
        server->wlrOutputLayout, wlrOutput
    );
    wlr_scene_output* sceneOutput = wlr_scene_output_create(server->wlrScene, wlrOutput);
    wlr_scene_output_layout_add_output(server->wlrSceneLayout, layoutOutput, sceneOutput);

    LOG_INFO("server new output added.")

    return 0;
}


void Output::frameEventHandler() {
    wlr_scene* scene = server->wlrScene;

    wlr_scene_output* sceneOutput = wlr_scene_get_scene_output(scene, wlrOutput);

    wlr_scene_output_commit(sceneOutput, nullptr);

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(sceneOutput, &now);

}

} // namespace vesper::desktop::server
