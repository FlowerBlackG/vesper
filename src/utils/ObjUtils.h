// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 面向对象开发时，一些好用的工具。 
 *
 * 创建于 2024年2月10日 北京市海淀区
 */

#pragma once

/**
 * 参考 Rust 的 xxx::new() 实现一个静态的构造函数。
 * 要求原类已经定义一个 init 函数，该函数返回非 0 值表示构造出错。
 * 参数应放置在一个结构体内（就像 vulkan 的风格那样）。
 */
#define VESPER_OBJ_UTILS_IMPL_CREATE(Type, OptionsType) \
    Type* Type::create(const OptionsType& options) { \
        auto* p = new (std::nothrow) Type; \
        if (p && p->init(options)) { \
            delete p; \
            p = nullptr; \
        } \
        \
        return p; \
    }


#define VESPER_OBJ_UTILS_DISABLE_COPY(Type) \
    Type(const Type&) = delete; \
    Type& operator = (const Type&) = delete;
