// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Framebuffer Plate
 * 
 * created on May 19, 2024
 * at Anting, Jiading, Shanghai
 */


#include "./FramebufferPlate.h"
#include "./Output.h"
#include "./Scene.h"
#include "../server/Server.h"
#include "../../utils/wlroots-cpp.h"
#include "../../log/Log.h"
#include <cstdint>
#include <libdrm/drm_fourcc.h>

using namespace vesper;
using namespace vesper::desktop;
using namespace vesper::bindings;
using namespace std;

namespace vesper::desktop::scene {


VESPER_OBJ_UTILS_IMPL_CREATE(FramebufferPlate, FramebufferPlate::CreateOptions)

int FramebufferPlate::init(const CreateOptions& options) {
    this->sceneOutput = options.sceneOutput;
    this->bufWidth = options.bufWidth;
    this->bufHeight = options.bufHeight;  // todo: should listen to display size change events.
    this->bytesPerPixel = options.bytesPerPixel;

    auto bufSize = bufHeight * bufWidth * bytesPerPixel;
    bool fbAllocSuccess = true;

    for (int i = 0; i < NBUF; i++) {
        buf[i].data = new (nothrow) int8_t[bufSize];
        if (!buf[i].data) {
            fbAllocSuccess = false;
            break;
        }
    }

    if (!fbAllocSuccess) {
        LOG_ERROR("framebuffer plate: failed to alloc frame buffers.")
        return -1;
    }

    return 0;
}


FramebufferPlate::~FramebufferPlate() {
    for (int i = 0; i < NBUF; i++) {
        if (buf[i].data) {
            delete[] buf[i].data;
            buf[i].data = nullptr;
        }
    }
}


void FramebufferPlate::put(wlr_buffer* buf, const pixman::Region32& damage) {
    std::lock_guard<std::mutex> lockGuard(this->lock);

    this->damage += damage;
    
    auto desktopServer = this->sceneOutput->scene->desktopServer;
    auto renderer = desktopServer->wlrRenderer;
    auto texture = wlr_texture_from_buffer(renderer, buf);
    if (!texture) {
        LOG_WARN("Vesper Desktop FramebufferPlate: failed to get texture from buffer.")
        return;
    }

    wlr_texture_read_pixels_options readPixelOpts {0};
    readPixelOpts.data = this->buf[writeToIdx].data;
    readPixelOpts.dst_x = 0;
    readPixelOpts.dst_y = 0;
    readPixelOpts.stride = bufWidth * bytesPerPixel;
    readPixelOpts.format = DRM_FORMAT_XRGB8888;
    
    auto success = wlr_texture_read_pixels(texture, &readPixelOpts);
    wlr_texture_destroy(texture);

    if (!success) {
        LOG_WARN("Vesper Desktop FramebufferPlate: failed to read pixels.")
        return;
    }

    if (this->availableBufIdx != this->writeToIdx) {
        this->availableBufIdx = this->writeToIdx;
    }
}


void* FramebufferPlate::get(vesper::bindings::pixman::Region32& damage) {
    std::lock_guard<std::mutex> lockGuard(this->lock);

    if (this->availableBufIdx == -1) {
        return nullptr;
    }

    this->writeToIdx ++;
    this->writeToIdx %= NBUF;

    auto res = (void*) buf[availableBufIdx].data;
    damage = this->damage;
    this->damage.clear();

    return res;
}



}  // namespace vesper::desktop::scene


