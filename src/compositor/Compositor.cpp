// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Wayland 合成器
 * 
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */


#include "Compositor.h"
#include "../log/Log.h"
using namespace std;


namespace vesper {

static void newOutputEventBridge(wl_listener* listener, void* data) {
    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newOutput);

    compositor->newOutputEventHandler((wlr_output*) data);
}

static void newXdgSurfaceEventBridge(wl_listener* listener, void* data) {

    Compositor* compositor = wl_container_of(listener, compositor, eventListeners.newOutput);
    compositor->newXdgSurfaceEventHandler((wlr_xdg_surface*) data);
}

Compositor::Compositor() {

}

Compositor::~Compositor() {
    this->clear();
}

int Compositor::init() {
    if (initialized) {
        return 0;
    }

    this->wlDisplay = wl_display_create();
    this->wlrBackend = wlr_backend_autocreate(wlDisplay, nullptr);
    if (!wlrBackend) {
        LOG_ERROR("failed to create wlroots backend!");
        return -1;
    }

    this->wlrRenderer = wlr_renderer_autocreate(wlrBackend);
    if (!wlrRenderer) {
        LOG_ERROR("failed to create wlroots renderer!");
        return -1;
    }

    wlr_renderer_init_wl_display(wlrRenderer, wlDisplay);

    wlrAllocator = wlr_allocator_autocreate(wlrBackend, wlrRenderer);
    if (!wlrAllocator) {
        LOG_ERROR("failed to create wlroots allocator!");
        return -1;
    }

    wlr_compositor_create(wlDisplay, 5, wlrRenderer);
    wlr_subcompositor_create(wlDisplay);
    wlr_data_device_manager_create(wlDisplay);

    wlrOutputLayout = wlr_output_layout_create();

    wl_list_init(&wlOutputs);

    eventListeners.newOutput.notify = newOutputEventBridge;
    wl_signal_add(&wlrBackend->events.new_output, &eventListeners.newOutput);

    wlrScene = wlr_scene_create();
    wlrSceneLayout = wlr_scene_attach_output_layout(wlrScene, wlrOutputLayout);

    // xdg shell
    wl_list_init(&wlViews);
    wlrXdgShell = wlr_xdg_shell_create(wlDisplay, 3);
    eventListeners.newXdgSurface.notify = newXdgSurfaceEventBridge;
    // todo

    LOG_INFO("compositor initialized.");
    return 0;
}


int Compositor::clear() {

    if (!initialized) {
        return 0;
    }

    if (false && wlDisplay) {
        wl_display_destroy_clients(wlDisplay);
        wl_display_destroy(wlDisplay);
        wlDisplay = nullptr;
    }
    return 0;
}

int Compositor::run() {



    return 0;
}


void Compositor::newOutputEventHandler(wlr_output* newOutput) {
    wlr_output_init_render(newOutput, wlrAllocator, wlrRenderer);

    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    
    wlr_output_mode* mode = wlr_output_preferred_mode(newOutput);

    if (mode) {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_commit_state(newOutput, &state);
    wlr_output_state_finish(&state);

    Output* output = new (nothrow) Output;
    if (!output) {
        LOG_ERROR("failed to create output!");
        // todo
    }

    output->init(this, newOutput);


}

void Compositor::newXdgSurfaceEventHandler(wlr_xdg_surface* newXdgSurface) {
    // todo
}


}
