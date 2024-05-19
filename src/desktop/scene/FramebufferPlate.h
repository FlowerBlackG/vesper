// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Framebuffer Plate
 * 
 * created on May 19, 2024
 * at Anting, Jiading, Shanghai
 */

#pragma once

#include <mutex>
#include "../../bindings/pixman.h"
#include "../../utils/ObjUtils.h"

namespace vesper::desktop::scene { class Scene; }
namespace vesper::desktop::scene { class Output; }
struct wlr_buffer;


namespace vesper::desktop::scene {



/**
 * Only one user is allowed to use one plate, which means when
 * method `get` is called, we assume that the previous rented framebuffer
 * can be reused.
 */
class FramebufferPlate {
public:
    struct CreateOptions {
        vesper::desktop::scene::Output* sceneOutput;
        int bufWidth;
        int bufHeight;
        int bytesPerPixel;
    };

    static FramebufferPlate* create(const CreateOptions&);

    ~FramebufferPlate();


    void put(wlr_buffer* buf, const vesper::bindings::pixman::Region32& damage);
    void* get(vesper::bindings::pixman::Region32& damage);


protected:
    VESPER_OBJ_UTILS_DISABLE_COPY(FramebufferPlate)

    FramebufferPlate() = default;
    int init(const CreateOptions&);


    scene::Output* sceneOutput;
    int bufWidth;
    int bufHeight;
    int bytesPerPixel;

    static const int NBUF = 2;
    struct {
        int8_t* data = nullptr;
    } buf[NBUF];

    vesper::bindings::pixman::Region32 damage;
    
    int availableBufIdx = -1;
    int writeToIdx = 0;

    std::mutex lock;
};


}  // namespace vesper::desktop::scene

