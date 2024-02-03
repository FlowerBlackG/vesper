// SPDX-License-Identifier: MulanPSL-2.0

/*
 * pixman c++ 封装
 * 
 * 创建于 2024年2月3日 北京市海淀区
 */

#pragma once

#include <pixman-1/pixman.h>

#include "../../utils/wlroots-cpp.h"

namespace vesper::bindings::pixman {

class Region32 {

public:
    Region32();
    ~Region32();


    bool empty();
    bool notEmpty();

    bool intersectRect(
        Region32& source, int x, int y, unsigned int width, unsigned int height
    );

    bool intersectRect(
        pixman_region32_t* source, int x, int y, unsigned int width, unsigned int height
    );

    bool intersectRect(
        Region32& source, wlr_box& box
    );

    bool intersectRect(
        pixman_region32_t* source, wlr_box& box
    );

    bool subtract(Region32& m, Region32& s);
    bool subtract(pixman_region32_t* m, Region32& s);
    bool subtract(Region32& m, pixman_region32_t* s);
    bool subtract(pixman_region32_t* m, pixman_region32_t* s);

    pixman_box32_t* rectangles(int* nRects);
    uint32_t regionArea();

protected:
    pixman_region32_t region32;

public:
    inline pixman_region32_t* raw() { return &region32; }

};

}
