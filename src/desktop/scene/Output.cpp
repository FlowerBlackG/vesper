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
#include "../../utils/ObjUtils.h"

#include "../../bindings/pixman/Region32.h"
#include "../server/Server.h"
#include "./FramebufferPlate.h"

#include <semaphore>


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
            output->pendingCommitDamage -= state->damage;
        } else {
            output->pendingCommitDamage += state->damage;
        }
    }
}


static void outputDamageEventBridge(wl_listener* listener, void* data) {
    Output* output = wl_container_of(listener, output, eventListeners.outputDamage);
    auto* event = (wlr_output_event_damage*) data;
    if (wlr_damage_ring_add(&output->wlrDamageRing, event->damage)) {
        output->scheduleFrame();
    }
}


static void outputNeedsFrameEventBridge(wl_listener* listener, void* data) {
    Output* output = wl_container_of(listener, output, eventListeners.outputNeedsFrame);

    output->scheduleFrame();
}


static void outputDestroyEventBridge(wlr_addon* addon) {
    Output* output = wl_container_of(addon, output, addon);
    delete output;
}


wlr_addon_interface sceneOutputAddonImpl = {
    .name = "vesper_scene_output",
    .destroy = outputDestroyEventBridge
};


VESPER_OBJ_UTILS_IMPL_CREATE(Output, Output::CreateOptions)


int Output::init(const CreateOptions& options) {
    this->wlrOutput = options.wlrOutput;
    this->scene = options.scene;

    this->alwaysRenderEntireScreen = options.alwaysRenderEntireScreen;
    this->exportScreenBuffer = options.exportScreenBuffer;
    this->forceRenderSoftwareCursor = options.forceRenderSoftwareCursor;

    
    if (this->exportScreenBuffer) {
        this->framebufferPlate = FramebufferPlate::create({
            .sceneOutput = this,
            .bufWidth = wlrOutput->width,
            .bufHeight = wlrOutput->height,
            .bytesPerPixel = 4  // todo: should get this number from somewhere else.
        });

        if (!this->framebufferPlate) {
            LOG_ERROR("failed to prepare framebuffer plate.")
            return -1;
        }
    }


    wlr_addon_init(&this->addon, &wlrOutput->addons, scene, &sceneOutputAddonImpl);
    
    wlr_damage_ring_init(&this->wlrDamageRing);
   
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

void Output::updateGeometry(bool forceUpdate, bool dontTouchTreeAndFrame) {
    
    wlr_damage_ring_add_whole(&wlrDamageRing);
    
    if (dontTouchTreeAndFrame) {
        return;
    }
    
    // then, we touch tree and frame :D
    
    this->scene->tree->outputUpdate(
        &this->scene->outputs, 
        nullptr, 
        forceUpdate ? this : nullptr
    );

    this->scheduleFrame();
}

void Output::scheduleFrame() {
    wlr_output_schedule_frame(this->wlrOutput);
}


bool Output::commit(StateOptions* options) {
    
    if (!wlrOutput->needs_frame && pendingCommitDamage.empty()) {
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
    std::vector<Output::RenderListEntry>* renderList;
    bool calculateVisibility;
    size_t nodeCount;
};


static bool constructRenderListIterator(
    SceneNode* node, int x, int y, void* untypedData
) {
    auto* data = (RenderListConstructorData*) untypedData;
    
    if (node->invisible()) {
        return false;
    }

    pixman::Region32 intersection;
    intersection.intersectRect(node->visibleArea, data->box);
    if (intersection.empty() && false) { // todo

        return false;
    }

    // 添加一个待渲染节点。

    auto& renderList = *(data->renderList);

    if (data->renderList->size() <= data->nodeCount) {
        data->renderList->emplace_back();
    }

    if (data->renderList->size() > data->nodeCount) {

        auto& entry = renderList[data->nodeCount];
        entry = {
            .node = node,
            .sentDmaBufFeedback = false,
            .x = x,
            .y = y
        };

        data->nodeCount++;
    }
    
    return false;
}


static void transformOutputDamage(pixman_region32_t* damage, const RenderData* data) {
    wl_output_transform transform = wlr_output_transform_invert(data->transform);
    wlr_region_transform(damage, damage, transform, data->transWidth, data->transHeight);
}


static void outputStateApplyDamage(const RenderData* data, wlr_output_state* state) {
    Output* output = data->output;

    pixman::Region32 frameDamage = &output->wlrDamageRing.current;
    transformOutputDamage(frameDamage.raw(), data);

    output->pendingCommitDamage += frameDamage;

    wlr_output_state_set_damage(state, output->pendingCommitDamage.raw());
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


static void sceneEntryRender(Output::RenderListEntry& entry, const RenderData& data) {

    const auto scaleLength = [] (int length, int offset, float scale) {
        return round((offset + length) * scale) - round(offset * scale);
    };

    SceneNode* node = entry.node;
    
    pixman::Region32 renderRegion = node->visibleArea;
    pixman_region32_translate(renderRegion.raw(), -data.logical.x, -data.logical.y);
    scaleOutputDamage(renderRegion.raw(), data.scale);
    renderRegion.intersectWith(data.damage);
    if (renderRegion.empty() && false) { // todo
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
            options.clip = nullptr; // todo : renderRegion.raw();
            options.filter_mode = WLR_SCALE_FILTER_BILINEAR;
            options.alpha = &buf->opacity;

            if (true) {
                options.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
            } else /* todo : 强制启用 premultiplied */ if (opaque.empty()) {
                options.blend_mode = WLR_RENDER_BLEND_MODE_NONE;
            } else {
                options.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
            }
            
            wlr_render_pass_add_texture(data.wlrRenderPass, &options);
            
            OutputSampleEvent sampleEvent = {
                .output = data.output
            };

            wl_signal_emit_mutable(&buf->events.outputSample, &sampleEvent);
        }
    }
}


static void forceRenderSoftwareCursorToRenderPass(
    wlr_output* output, wlr_render_pass* pass, pixman_region32_t* damage
) {

    int width, height;
    wlr_output_transformed_resolution(output, &width, &height);

    pixman::Region32 renderDamage = (wlr_box) {
        .x = 0, .y = 0, .width = width, .height = height
    };
    
    if (damage) {
        renderDamage.intersectWith(damage);
    }

    wlr_output_cursor* cursor;
    wl_list_for_each(cursor, &output->cursors, link) {
        if (!cursor->enabled || !cursor->visible) {
            continue;
        }

        auto& texture = cursor->texture;
        if (texture == nullptr) {
            continue;
        }

        wlr_box box {
            .x = int(cursor->x - cursor->hotspot_x),
            .y = int(cursor->y - cursor->hotspot_y),
            .width = int(cursor->width),
            .height = int(cursor->height)
        };

        pixman::Region32 cursorDamage = box;
        cursorDamage.intersectWith(renderDamage);

        if (cursorDamage.empty()) {
            continue;
        }

        wl_output_transform transform = wlr_output_transform_invert(output->transform);
        wlr_box_transform(&box, &box, transform, width, height);
        wlr_region_transform(cursorDamage.raw(), cursorDamage.raw(), transform, width, height);

        wlr_render_texture_options renderOptions = {
            .texture = texture,
            .src_box = cursor->src_box,
            .dst_box = box,
            .clip = cursorDamage.raw(),
            .transform = output->transform
        };

        wlr_render_pass_add_texture(pass, &renderOptions);
    }
}


bool Output::buildState(wlr_output_state* state, StateOptions* options) {

    if (alwaysRenderEntireScreen || true) { // todo: fix damage calculation
        this->updateGeometry(true, true);
    }

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
        .calculateVisibility = this->scene->calculateVisibility,
        .nodeCount = 0
    };


    this->scene->tree->nodesInBox(&listCon.box, constructRenderListIterator, &listCon);

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

    // 先取出整个需要重新渲染的区域
    
    wlr_damage_ring_rotate_buffer(&wlrDamageRing, buffer, renderData.damage.raw());


    // 假设要为整个重渲染区域绘制 background
    pixman::Region32 background = renderData.damage;

    if (this->scene->calculateVisibility) {
        for (int i = listCon.nodeCount - 1; i >= 0; i--) {
            RenderListEntry& entry = (*listCon.renderList)[i];

            pixman::Region32 opaque;
            entry.node->opaqueRegion(entry.x, entry.y, opaque.raw());
            opaque.intersectWith(entry.node->visibleArea);

            pixman_region32_translate(opaque.raw(), -position.x, -position.y);
            wlr_region_scale(opaque.raw(), opaque.raw(), renderData.scale);
            background -= opaque;  // 有窗口内容的地方不用画 background
        }

        if (floor(renderData.scale) != renderData.scale) {
            wlr_region_expand(background.raw(), background.raw(), 1);
            background.intersectWith(renderData.damage);
        }
    }

    transformOutputDamage(background.raw(), &renderData);


    // 渲染桌面背景

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
        },
        .clip = background.raw()
    };
    wlr_render_pass_add_rect(renderPass, &backgroundRenderRectOptions);


    // 渲染每个 view
    
    for (int i = listCon.nodeCount - 1; i >= 0; i--) {
        RenderListEntry& entry = (*listCon.renderList)[i];
        sceneEntryRender(entry, renderData);
        
        // 发送 dmabuf 信息

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

    // software cursor 

    if (this->forceRenderSoftwareCursor) {
        forceRenderSoftwareCursorToRenderPass(wlrOutput, renderPass, renderData.damage.raw());
    }


    if (!wlr_render_pass_submit(renderPass)) {
        wlr_buffer_unlock(buffer);
        wlr_damage_ring_add_whole(&wlrDamageRing);
        return false;
    }

    wlr_output_state_set_buffer(state, buffer);

    if (exportScreenBuffer) {
        this->framebufferPlate->put(buffer, renderData.damage);
    }

    wlr_buffer_unlock(buffer);

    return true;
    
}  // bool Output::buildState(wlr_output_state* state, StateOptions* options)


void Output::sendFrameDone(timespec* now) {
    this->scene->tree->sendFrameDone(this, now);
}


Output::~Output() {

    wl_signal_emit_mutable(&events.destroy, nullptr);

    this->scene->tree->outputUpdate(&scene->outputs, this, nullptr);

    // todo: highlight region

    wlr_addon_finish(&this->addon);
    wlr_damage_ring_finish(&wlrDamageRing);

    wl_list_remove(&link);
    wl_list_remove(&eventListeners.outputCommit.link);
    wl_list_remove(&eventListeners.outputDamage.link);
    wl_list_remove(&eventListeners.outputNeedsFrame.link);

    if (this->framebufferPlate) {
        delete this->framebufferPlate;
        this->framebufferPlate = nullptr;
    }

}


}  // namespace vesper::desktop::scene 




