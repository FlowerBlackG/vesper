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
    Region32(const Region32& src);
    Region32(pixman_region32_t* cObj);
    Region32(pixman_region32_t& cObj);
    Region32(const wlr_box& box);

    ~Region32();

    void clear();


    bool empty();
    bool notEmpty();

    bool intersectWith(const pixman_region32_t* other);
    bool intersectWith(const pixman_region32_t& other);
    bool intersectWith(const Region32& other);

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
        Region32& source, wlr_fbox& box
    );

    bool intersectRect(
        pixman_region32_t* source, wlr_box& box
    );

    bool subtract(Region32& m, Region32& s);
    bool subtract(pixman_region32_t* m, Region32& s);
    bool subtract(Region32& m, pixman_region32_t* s);
    bool subtract(pixman_region32_t* m, pixman_region32_t* s);

    void translate(int x, int y);

    pixman_box32_t* rectangles(int* nRects);
    const pixman_box32_t* rectangles(int* nRects) const;
    uint32_t regionArea();

public:

    Region32& operator = (const Region32& other);
    Region32& operator = (const pixman_region32_t* other);

    Region32& operator -= (const Region32& other);
    Region32& operator -= (const pixman_region32_t* other);
    Region32& operator -= (const pixman_region32_t& other);
    Region32& operator += (const Region32& other);
    Region32& operator += (const pixman_region32_t* other);
    Region32& operator += (const pixman_region32_t& other);
    Region32& operator += (const pixman_box32_t& other);

    bool operator == (const Region32& other) const;
    bool operator == (const pixman_region32_t* other) const;
    bool operator == (const pixman_region32_t& other) const;

protected:
    pixman_region32_t region32;

public:
    inline pixman_region32_t* raw() { return &region32; }
    inline const pixman_region32_t* raw() const { return &region32; }

};

}
