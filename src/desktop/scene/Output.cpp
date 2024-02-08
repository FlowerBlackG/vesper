// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Scene Output
 * 
 * 创建于 2024年1月29日 江西省上饶市玉山县
 */

#include "./Output.h"
#include "./Scene.h"
#include "./SceneNode.h"
#include "./RenderTimer.h"
#include "./RenderData.h"

#include "../../log/Log.h"

#include "../../bindings/pixman/Region32.h"
#include <jpeglib.h>
using namespace std;
using namespace vesper::bindings;

namespace vesper::desktop::scene {


static void outputCommitEventBridge(wl_listener* listener, void* data) {
    Output* output = wl_container_of(listener, output, eventListeners.outputCommit);
    auto* event = (wlr_output_event_commit*) data;
    const wlr_output_state* state = event->state;

    bool forceUpdate = state->committed & (
        WLR_OUTPUT_STATE_TRANSFORM | 
        WLR_OUTPUT_STATE_SCALE | 
        WLR_OUTPUT_STATE_SUBPIXEL
    );

    if (
        forceUpdate 
        || (state->committed & (WLR_OUTPUT_STATE_MODE | WLR_OUTPUT_STATE_ENABLED))
    ) {
        output->updateGeometry(forceUpdate);
    }

    if (state->committed & WLR_OUTPUT_STATE_DAMAGE) {
        if (wlr_swapchain_has_buffer(output->wlrOutput->swapchain, state->buffer)) {
            pixman_region32_subtract(
                &output->pendingCommitDamage, 
                &output->pendingCommitDamage, 
                &state->damage
            );
        } else {
            pixman_region32_union(
                &output->pendingCommitDamage,
                &output->pendingCommitDamage,
                &state->damage
            );
        }
    }
}


static void outputDamageEventBridge(wl_listener* listener, void* data) {
    Output* output = wl_container_of(listener, output, eventListeners.outputDamage);
    auto* event = (wlr_output_event_damage*) data;
    if (wlr_damage_ring_add(&output->wlrDamageRing, event->damage)) {
        wlr_output_schedule_frame(output->wlrOutput);
    }
}


static void outputNeedsFrameEventBridge(wl_listener* listener, void* data) {
    Output* output = wl_container_of(listener, output, eventListeners.outputNeedsFrame);
    wlr_output_schedule_frame(output->wlrOutput);
}


static void outputDestroyEventBridge(wlr_addon* addon) {
    Output* output = wl_container_of(addon, output, addon);
    output->destroy();
    delete output;
}


wlr_addon_interface sceneOutputAddonImpl = {
    .name = "vesper_scene_output",
    .destroy = outputDestroyEventBridge
};


Output* Output::create(Scene* scene, wlr_output* wlrOutput) {
    auto* p = new (nothrow) Output;
    if (!p) {
        return nullptr;
    }

    if (p->init(scene, wlrOutput)) {
        delete p;
        return nullptr;
    }

    return p;
}


int Output::init(Scene* scene, wlr_output* wlrOutput) {
    this->wlrOutput = wlrOutput;
    this->scene = scene;

    wl_array_init(&this->renderList);

    wlr_addon_init(&this->addon, &wlrOutput->addons, scene, &sceneOutputAddonImpl);
    
    wlr_damage_ring_init(&this->wlrDamageRing);
    pixman_region32_init(&this->pendingCommitDamage);
   
    int prevOutputIndex = -1;
    wl_list* prevOutputLink = &scene->outputs;

    Output* currentOutput;
    wl_list_for_each(currentOutput, &scene->outputs, link) {
        if (prevOutputIndex + 1 != currentOutput->index) {
            break;
        }

        prevOutputIndex = currentOutput->index;
        prevOutputLink = &currentOutput->link;
    }

    this->index = prevOutputIndex + 1;

    wl_list_insert(prevOutputLink, &this->link);

    wl_signal_init(&events.destroy);

    eventListeners.outputCommit.notify = outputCommitEventBridge;
    wl_signal_add(&wlrOutput->events.commit, &eventListeners.outputCommit);

    eventListeners.outputDamage.notify = outputDamageEventBridge;
    wl_signal_add(&wlrOutput->events.damage, &eventListeners.outputDamage);
    
    eventListeners.outputNeedsFrame.notify = outputNeedsFrameEventBridge;
    wl_signal_add(&wlrOutput->events.needs_frame, &eventListeners.outputNeedsFrame);

    this->updateGeometry(false);

    return 0;
}


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


bool Output::commit(StateOptions* options) {

    if (!wlrOutput->needs_frame && pixman_region32_empty(&pendingCommitDamage)) {
        return true;
    }

    wlr_output_state state;
    wlr_output_state_init(&state);

    if (!this->buildState(&state, options)) {
        wlr_output_state_finish(&state);
        return false;
    }

    bool ok = wlr_output_commit_state(this->wlrOutput, &state);

    wlr_output_state_finish(&state);
    return ok;
}


static void wlrOutputPendingResolution(
    wlr_output* output, wlr_output_state* state, int* width, int* height
) {
    if (state->committed & WLR_OUTPUT_STATE_MODE) {
        if (state->mode_type == WLR_OUTPUT_STATE_MODE_FIXED) {
            *width = state->mode->width;
            *height = state->mode->height;
        } else if (state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
            *width = state->custom_mode.width;
            *height = state->custom_mode.height;
        }
    } else {
        *width = output->width;
        *height = output->height;
    }
}


struct RenderListConstructorData {
    wlr_box box;
    wl_array* renderList;
    bool calculateVisibility;
};


struct RenderListEntry {
    SceneNode* node;
    bool sentDmaBufFeedback;
    int x, y;
};


static bool constructRenderListIterator(
    SceneNode* node, int x, int y, void* untypedData
) {
    auto* data = (RenderListConstructorData*) untypedData;
    
    if (node->invisible()) {
        return false;
    }

    pixman::Region32 intersection;
    intersection.intersectRect(&node->visibleArea, data->box);
    if (intersection.empty()) {
        return false;
    }


    RenderListEntry* entry = (RenderListEntry*) wl_array_add(data->renderList, sizeof(*entry));
    if (!entry) {
        return false;
    }

    entry->node = node;
    entry->x = x;
    entry->y = y;

    return false;
}


static bool wlArrayRealloc(wl_array* arr, size_t size) {
    size_t alloc;
    if (arr->alloc > 0 && size > arr->alloc / 4) {
        alloc = arr->alloc;
    } else {
        alloc = 16;
    }

    while (alloc < size) {
        alloc *= 2;
    }

    if (alloc == arr->alloc) {
        return true;
    }


    void* data = realloc(arr->data, alloc);
    if (data == nullptr) {
        return false;
    }

    arr->data = data;
    arr->alloc = alloc;
    return true;
}


static void transformOutputDamage(pixman_region32_t* damage, const RenderData* data) {
    wl_output_transform transform = wlr_output_transform_invert(data->transform);
    wlr_region_transform(damage, damage, transform, data->transWidth, data->transHeight);
}


static void outputStateApplyDamage(const RenderData* data, wlr_output_state* state) {
    Output* output = data->output;

    pixman::Region32 frameDamage;
    pixman_region32_copy(frameDamage.raw(), &output->wlrDamageRing.current);
    transformOutputDamage(frameDamage.raw(), data);
    pixman_region32_union(
        &output->pendingCommitDamage, 
        &output->pendingCommitDamage, 
        frameDamage.raw()
    );

    wlr_output_state_set_damage(state, &output->pendingCommitDamage);
}


static void timespecSub(timespec* result, timespec* a, timespec* b) {
    result->tv_sec = a->tv_sec - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0) {
        result->tv_sec--;
        result->tv_nsec += 1000000000; // 1s = 1e9 ns
    }
}


static int64_t timespecToNsec(const timespec* a) {
    return ((int64_t) a->tv_sec) * 1000000000 + a->tv_nsec;
}


static void scaleOutputDamage(pixman_region32_t* damage, float scale) {
    wlr_region_scale(damage, damage, scale);
    if (floor(scale) != scale) {
        wlr_region_expand(damage, damage, 1);
    }
}


static void sceneEntryRender(RenderListEntry& entry, const RenderData& data) {

    const auto scaleLength = [] (int length, int offset, float scale) {
        return round((offset + length) * scale) - round(offset * scale);
    };

    SceneNode* node = entry.node;
    
    pixman::Region32 renderRegion;
    pixman_region32_copy(renderRegion.raw(), &node->visibleArea);
    pixman_region32_translate(renderRegion.raw(), -data.logical.x, -data.logical.y);
    scaleOutputDamage(renderRegion.raw(), data.scale);
    pixman_region32_intersect(renderRegion.raw(), renderRegion.raw(), &data.damage);
    if (renderRegion.empty()) {
        return;
    }

    wlr_box dstBox = {
        .x = entry.x - data.logical.x,
        .y = entry.y - data.logical.y
    };
    node->getSize(&dstBox.width, &dstBox.height);
    
    dstBox.width = scaleLength(dstBox.width, dstBox.x, data.scale);
    dstBox.height = scaleLength(dstBox.height, dstBox.y, data.scale);
    dstBox.x = round(dstBox.x * data.scale);
    dstBox.y = round(dstBox.y * data.scale);

    pixman::Region32 opaque;
    node->opaqueRegion(dstBox.x, dstBox.y, opaque.raw());
    scaleOutputDamage(opaque.raw(), data.scale);
    opaque.subtract(renderRegion, opaque);

    wl_output_transform transform = wlr_output_transform_invert(data.transform);
    wlr_box_transform(&dstBox, &dstBox, transform, data.transWidth, data.transHeight);
    transformOutputDamage(renderRegion.raw(), &data);

    if (node->type() == SceneNodeType::TREE) {
        LOG_ERROR("bad type!");
    } else if (node->type() == SceneNodeType::RECT) {
        auto* rect = (SceneRectNode*) node;
        auto options = (wlr_render_rect_options) {
            .box = dstBox,
            .color = {
                .r = rect->color[0],
                .g = rect->color[1],
                .b = rect->color[2],
                .a = rect->color[3],
            },
            .clip = renderRegion.raw()
        };

        wlr_render_pass_add_rect(data.wlrRenderPass, &options);
    } else if (node->type() == SceneNodeType::BUFFER) {
        auto* buf = (SceneBufferNode*) node;
        wlr_texture* texture = buf->getTexture(data.output->wlrOutput->renderer);
        if (texture) {
            wl_output_transform bufTransform = wlr_output_transform_invert(buf->transform);
            bufTransform = wlr_output_transform_compose(bufTransform, data.transform);

            wlr_render_texture_options options;
            options.texture = texture;
            options.src_box = buf->srcBox;
            options.dst_box = dstBox;
            options.transform = bufTransform;
            options.clip = renderRegion.raw();
            options.filter_mode = WLR_SCALE_FILTER_BILINEAR;
            options.alpha = &buf->opacity;

            if (opaque.empty()) {
                options.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
            } else {
                options.blend_mode = WLR_RENDER_BLEND_MODE_NONE;
            }
            
            wlr_render_pass_add_texture(data.wlrRenderPass, &options);
            
            OutputSampleEvent sampleEvent = {
                .output = data.output
            };

            wl_signal_emit_mutable(&buf->events.outputSample, &sampleEvent);
        }
    }
}


bool Output::buildState(wlr_output_state* state, StateOptions* options) {

    this->updateGeometry(true); // todo

    StateOptions defaultOptions = {
        .timer = nullptr
    };

    if (!options) {
        options = &defaultOptions;
    }

    RenderTimer* timer = options->timer;
    timespec startTime;

    if (timer) {
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        if (timer->wlrRenderTimer) {
            wlr_render_timer_destroy(timer->wlrRenderTimer);
        }

        *timer = {
            .preRenderDuration = 0,
            .wlrRenderTimer = nullptr
        };
    }

    if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && !state->enabled) {
        return true; // 对于已经 disable 的状态，不用处理。
    }

    RenderData renderData = {
        .transform = wlrOutput->transform,
        .scale = wlrOutput->scale,
        .logical = {
            .x = this->position.x,
            .y = this->position.y
        },
        .output = this
    };

    wlrOutputPendingResolution(
        wlrOutput, state, &renderData.transWidth, &renderData.transHeight
    );

    if (state->committed & WLR_OUTPUT_STATE_TRANSFORM) {
        if (renderData.transform != state->transform) {
            wlr_damage_ring_add_whole(&wlrDamageRing);
        }

        renderData.transform = state->transform;
    }

    if (state->committed & WLR_OUTPUT_STATE_SCALE) {
        if (renderData.scale != state->scale) {
            wlr_damage_ring_add_whole(&wlrDamageRing);
        }

        renderData.scale = state->scale;
    }

    wlr_output_transform_coords(
        renderData.transform, &renderData.transWidth, &renderData.transHeight
    );

    renderData.logical.width = renderData.transWidth / renderData.scale;
    renderData.logical.height = renderData.transHeight / renderData.scale;

    RenderListConstructorData listCon = {
        .box = renderData.logical,
        .renderList = &this->renderList,
        .calculateVisibility = this->scene->calculateVisibility
    };

    listCon.renderList->size = 0;
    this->scene->tree->nodesInBox(&listCon.box, constructRenderListIterator, &listCon);
    wlArrayRealloc(listCon.renderList, listCon.renderList->size);

    RenderListEntry* listData = (RenderListEntry*) listCon.renderList->data;
    int listLen = listCon.renderList->size / sizeof(*listData);

    outputStateApplyDamage(&renderData, state);


    wlr_damage_ring_set_bounds(&wlrDamageRing, renderData.transWidth, renderData.transHeight);
    if (!wlr_output_configure_primary_swapchain(wlrOutput, state, &wlrOutput->swapchain)) {
        return false;
    }

    wlr_buffer* buffer = wlr_swapchain_acquire(wlrOutput->swapchain, nullptr);
    if (buffer == nullptr) {
        return false;
    }

    if (timer) {
        timer->wlrRenderTimer = wlr_render_timer_create(wlrOutput->renderer);

        timespec endTime, duration;
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        timespecSub(&duration, &endTime, &startTime);
        timer->preRenderDuration = timespecToNsec(&duration);
    }

    auto bufRenderPassOptions = (wlr_buffer_pass_options) {
        .timer = timer ? timer->wlrRenderTimer : nullptr
    };
    wlr_render_pass* renderPass = wlr_renderer_begin_buffer_pass(
        wlrOutput->renderer, buffer, &bufRenderPassOptions
    );

    if (renderPass == nullptr) {
        wlr_buffer_unlock(buffer);
        return false;
    }

    renderData.wlrRenderPass = renderPass;

    pixman_region32_init(&renderData.damage);
    wlr_damage_ring_rotate_buffer(&wlrDamageRing, buffer, &renderData.damage);

    pixman::Region32 background;
    pixman_region32_copy(background.raw(), &renderData.damage);

    if (this->scene->calculateVisibility) {
        for (int i = listLen - 1; i >= 0; i--) {
            RenderListEntry& entry = listData[i];

            pixman::Region32 opaque;
            entry.node->opaqueRegion(entry.x, entry.y, opaque.raw());
            pixman_region32_intersect(opaque.raw(), opaque.raw(), &entry.node->visibleArea);

            pixman_region32_translate(opaque.raw(), -position.x, -position.y);
            wlr_region_scale(opaque.raw(), opaque.raw(), renderData.scale);
            background.subtract(background, opaque);
        }

        if (floor(renderData.scale) != renderData.scale) {
            wlr_region_expand(background.raw(), background.raw(), 1);
            pixman_region32_intersect(background.raw(), background.raw(), &renderData.damage);
        }
    }

    transformOutputDamage(background.raw(), &renderData);

    // 桌面背景
    
    auto backgroundRenderRectOptions = (wlr_render_rect_options) {
        .box = {
            .width = buffer->width,
            .height = buffer->height
        },
        .color = {
            .r = 0.54117f,
            .g = 0.73725f,
            .b = 0.81961f,
            .a = 1.f
        }
    };
    wlr_render_pass_add_rect(renderPass, &backgroundRenderRectOptions);

    for (int i = listLen - 1; i >= 0; i--) {
        RenderListEntry& entry = listData[i];
        sceneEntryRender(entry, renderData);
        
        if (entry.node->type() == SceneNodeType::BUFFER) {
            auto* buf = (SceneBufferNode*) entry.node;
            if (buf->primaryOutput == this && !entry.sentDmaBufFeedback) {
                wlr_linux_dmabuf_feedback_v1_init_options feedbackOptions = {
                    .main_renderer = wlrOutput->renderer,
                    .scanout_primary_output = nullptr
                };

                this->scene->sceneBufferSendDmaBufFeedback(buf, &feedbackOptions);
            }
        }
    }

    //wlr_output_add_software_cursors_to_render_pass(wlrOutput, renderPass, &renderData.damage);

    pixman_region32_fini(&renderData.damage);

    if (!wlr_render_pass_submit(renderPass)) {
        wlr_buffer_unlock(buffer);
        wlr_damage_ring_add_whole(&wlrDamageRing);
        return false;
    }

    




#if 0
wlr_renderer* pixmanRenderer = wlrOutput->renderer;
pixman_image_t* img = wlr_pixman_renderer_get_buffer_image(pixmanRenderer, buffer);
int width = pixman_image_get_width(img);
int height = pixman_image_get_height(img);
int stride = pixman_image_get_stride(img);
int depth = pixman_image_get_depth(img);
pixman_format_code_t fmt = pixman_image_get_format(img);
uint32_t* imgData = pixman_image_get_data(img);

static int __photoId = -1;
__photoId++;
string fname = std::to_string(__photoId);
fname += ".jpg";
fname = "./" + fname;
FILE* outfile = fopen(fname.c_str(), "wb");
jpeg_compress_struct cinfo;
jpeg_error_mgr jerr;
cinfo.err = jpeg_std_error(&jerr);
jpeg_create_compress(&cinfo);
jpeg_stdio_dest(&cinfo, outfile);
cinfo.image_width = width;
cinfo.image_height = height;
cinfo.in_color_space = JCS_EXT_BGRX;
cinfo.input_components = 4;
jpeg_set_defaults(&cinfo);
jpeg_start_compress(&cinfo, TRUE);
int i = 0;
while (cinfo.next_scanline < cinfo.image_height) {
    unsigned char* linePtr = (unsigned char*) imgData;
    linePtr += i * cinfo.image_width * cinfo.input_components;
    jpeg_write_scanlines(&cinfo, &linePtr, 1);
    i++;
}
jpeg_finish_compress(&cinfo);
jpeg_destroy_compress(&cinfo);
#endif


    wlr_output_state_set_buffer(state, buffer);
    wlr_buffer_unlock(buffer);

    return true;
}


void Output::sendFrameDone(timespec* now) {
    this->scene->tree->sendFrameDone(this, now);
}


void Output::destroy() {

    wl_signal_emit_mutable(&events.destroy, nullptr);

    this->scene->tree->outputUpdate(&scene->outputs, this, nullptr);

    // todo: highlight region

    wlr_addon_finish(&this->addon);
    wlr_damage_ring_finish(&wlrDamageRing);
    
    pixman_region32_fini(&pendingCommitDamage);

    wl_list_remove(&link);
    wl_list_remove(&eventListeners.outputCommit.link);
    wl_list_remove(&eventListeners.outputDamage.link);
    wl_list_remove(&eventListeners.outputNeedsFrame.link);

    wl_array_release(&this->renderList);
}

}
