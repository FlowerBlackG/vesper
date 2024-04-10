// SPDX-License-Identifier: MulanPSL-2.0

/*
 * vesper control socket 通信协议
 * 创建于 2024年3月6日 上海市嘉定区
 */

#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include "../log/Log.h"

#ifndef __packed
    #define __packed __attribute__((packed))
#endif


#define VESPER_CTRL_EXPAND_RECV_PROTOS(macro) \
    macro(TerminateVesper) \
    macro(GetVNCPort) \
    macro(GetVNCPassword)


namespace vesper::control::protocol {

inline const char* MAGIC_STR = "KpBL";
inline const int HEADER_LEN = 16;

#define VESPER_CTRL_PROTO_DECL_GET_TYPE() \
    virtual uint32_t getType() const override;

class Base {
public:
    static const uint32_t typeCode = 0;
    virtual uint32_t getType() const = 0;
    void encode(std::stringstream& container) const;

    virtual int decodeBody(const char* data, int len);

    virtual ~Base() {}
protected:
    virtual uint64_t bodyLength() const;
    virtual void encodeBody(std::stringstream& container) const;
};


class Response : public Base {
public:
    static const uint32_t typeCode = 0xA001;
    VESPER_CTRL_PROTO_DECL_GET_TYPE()
    uint32_t code;
    std::string msg;

protected:
    virtual uint64_t bodyLength() const;
    virtual void encodeBody(std::stringstream& container) const override;
};


class TerminateVesper : public Base {
public:
    static const uint32_t typeCode = 0x0001;
    VESPER_CTRL_PROTO_DECL_GET_TYPE()

    virtual int decodeBody(const char* data, int len) override;

protected:


};


class GetVNCPort : public Base {
public:
    static const uint32_t typeCode = 0x0101;
    VESPER_CTRL_PROTO_DECL_GET_TYPE()

    virtual int decodeBody(const char* data, int len) override;

protected:

};


class GetVNCPassword : public Base {
public:
    static const uint32_t typeCode = 0x0102;
    VESPER_CTRL_PROTO_DECL_GET_TYPE()

    virtual int decodeBody(const char* data, int len) override;

protected:

};


/**
 * 
 * 
 * @param data 指向报文开头。调用者需要确保 magic 正确。
 * @param len 整个报文长度。包含 header。调用者需要确保这个值不小于 16。
 * 
 * @return 失败时，返回 nullptr.
 */
Base* decode(const char* data, uint32_t type, int len);

#undef VESPER_CTRL_PROTO_DECL_GET_TYPE

} // namespace vesper::control::protocol
