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
#include "../scene/Output.h"
#include "../scene/OutputLayout.h"

#include "./Server.h"

using namespace std;
using namespace vesper::desktop;

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

    server->terminate();
}

VESPER_OBJ_UTILS_IMPL_CREATE(Output, Output::CreateOptions);

int Output::init(const CreateOptions& options) {

    this->server = options.server;
    this->wlrOutput = options.wlrOutput;

    eventListeners.frame.notify = frameEventBridge;
    wl_signal_add(&wlrOutput->events.frame, &eventListeners.frame);

    eventListeners.requestState.notify = requestStateEventBridge;
    wl_signal_add(&wlrOutput->events.request_state, &eventListeners.requestState);

    eventListeners.destroy.notify = destroyEventBridge;
    wl_signal_add(&wlrOutput->events.destroy, &eventListeners.destroy);

    wl_list_insert(&server->outputs, &link);

    wlr_output_layout_output* layoutOutput = wlr_output_layout_add_auto(
        server->wlrOutputLayout, wlrOutput
    );


    auto& outputOptions = server->options.output;
    sceneOutput = scene::Output::create({
        .scene = server->scene,
        .wlrOutput = wlrOutput,
        .alwaysRenderEntireScreen = outputOptions.alwaysRenderEntireScreen,
        .exportScreenBuffer = outputOptions.exportScreenBuffer,
        .forceRenderSoftwareCursor = outputOptions.forceRenderSoftwareCursor,
    });
    
    if (sceneOutput == nullptr) {
        LOG_ERROR("failed to create scene output!");
        return -1;
    }

    server->sceneLayout->addOutput(layoutOutput, sceneOutput);

    LOG_INFO("server new output added.")

    return 0;
}


void Output::frameEventHandler() {
    scene::Scene* scene = server->scene;

    scene::Output* sceneOutput = scene->getSceneOutput(this->wlrOutput);

    sceneOutput->commit(nullptr);
    
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    sceneOutput->sendFrameDone(&now);

}

} // namespace vesper::desktop::server
