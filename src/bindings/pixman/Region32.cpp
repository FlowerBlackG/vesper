// SPDX-License-Identifier: MulanPSL-2.0

/*
 * pixman c++ 封装
 * 
 * 创建于 2024年2月3日 北京市海淀区
 */


#include "./Region32.h"

namespace vesper::bindings::pixman {

Region32::Region32() {
    pixman_region32_init(raw());
}


Region32::Region32(const Region32& src) {
    pixman_region32_init(raw());
    pixman_region32_copy(raw(), src.raw());
}

Region32::Region32(pixman_region32_t* cObj) {
    pixman_region32_init(raw());
    pixman_region32_copy(raw(), cObj);
}

Region32::Region32(pixman_region32_t& cObj) {
    pixman_region32_init(raw());
    pixman_region32_copy(raw(), &cObj);
}

Region32::Region32(const wlr_box& box) {
    pixman_region32_init_rect(raw(), box.x, box.y, box.width, box.height);
}

Region32::~Region32() {
    pixman_region32_fini(raw());
}

void Region32::clear() {
    pixman_region32_clear(raw());
}


bool Region32::empty() {
    return !!pixman_region32_empty(raw());
}


bool Region32::notEmpty() {
    return !!pixman_region32_not_empty(raw());
}

bool Region32::intersectWith(const pixman_region32_t* other) {
    return !!pixman_region32_intersect(raw(), raw(), other);
}

bool Region32::intersectWith(const pixman_region32_t& other) {
    return !!pixman_region32_intersect(raw(), raw(), &other);
}

bool Region32::intersectWith(const Region32& other) {
    return !!pixman_region32_intersect(raw(), raw(), other.raw());
}


bool Region32::intersectRect(
    Region32& source, int x, int y, unsigned int width, unsigned int height
) {
    return intersectRect(source.raw(), x, y, width, height);
}


bool Region32::intersectRect(
    pixman_region32_t* source, int x, int y, unsigned int width, unsigned int height
) {
    return !!pixman_region32_intersect_rect(raw(), source, x, y, width, height);
}


bool Region32::intersectRect(
    Region32& source, wlr_box& box
) {
    return intersectRect(source.raw(), box);
}


bool Region32::intersectRect(
    Region32& source, wlr_fbox& box
) {
    return intersectRect(source, box.x, box.y, box.width, box.height);
}


bool Region32::intersectRect(
    pixman_region32_t* source, wlr_box& box
) {
    return intersectRect(source, box.x, box.y, box.width, box.height);
}


bool Region32::subtract(Region32& m, Region32& s) {
    return subtract(m.raw(), s.raw());
}


bool Region32::subtract(pixman_region32_t* m, Region32& s) {
    return subtract(m, s.raw());
}


bool Region32::subtract(Region32& m, pixman_region32_t* s) {
    return subtract(m.raw(), s);
}


bool Region32::subtract(pixman_region32_t* m, pixman_region32_t* s) {
    return !!pixman_region32_subtract(raw(), m, s);
}


void Region32::translate(int x, int y) {
    pixman_region32_translate(raw(), x, y);
}

pixman_box32_t* Region32::rectangles(int* nRects) {
    return pixman_region32_rectangles(raw(), nRects);
}

const pixman_box32_t* Region32::rectangles(int* nRects) const {
    return pixman_region32_rectangles(raw(), nRects);
}

uint32_t Region32::regionArea() {
    uint32_t area = 0;
    int nRects;
    pixman_box32_t* rects = rectangles(&nRects);
    
    for (int idx = 0; idx < nRects; idx++) {
        pixman_box32_t& rect = rects[idx];
        area += (rect.x2 - rect.x1) * (rect.y2 - rect.y1);
    }

    return area;
}


Region32& Region32::operator = (const Region32& other) {
    pixman_region32_copy(raw(), other.raw());
    return *this;
}


Region32& Region32::operator = (const pixman_region32_t* other) {
    pixman_region32_copy(raw(), other);
    return *this;
}


Region32& Region32::operator -= (const Region32& other) {
    pixman_region32_subtract(raw(), raw(), other.raw());
    return *this;    
}


Region32& Region32::operator -= (const pixman_region32_t* other) {
    pixman_region32_subtract(raw(), raw(), other);
    return *this;    
}

Region32& Region32::operator -= (const pixman_region32_t& other) {
    pixman_region32_subtract(raw(), raw(), &other);
    return *this;    
}

Region32& Region32::operator += (const Region32& other) {
    pixman_region32_union(raw(), raw(), other.raw());
    return *this;
}


Region32& Region32::operator += (const pixman_region32_t* other) {
    pixman_region32_union(raw(), raw(), other);
    return *this;
}

Region32& Region32::operator += (const pixman_region32_t& other) {
    pixman_region32_union(raw(), raw(), &other);
    return *this;
}


Region32& Region32::operator += (const pixman_box32_t& other) {
    pixman_region32_union_rect(
        raw(), raw(), 
        other.x1, 
        other.y1, 
        other.x2 - other.x1, 
        other.y2 - other.y1
    );
    return *this;
}


bool Region32::operator == (const Region32& other) const {
    return pixman_region32_equal(raw(), other.raw());    
}


bool Region32::operator == (const pixman_region32_t* other) const {
    return pixman_region32_equal(raw(), other);    
}


bool Region32::operator == (const pixman_region32_t& other) const {
    return pixman_region32_equal(raw(), &other);    
}


}
