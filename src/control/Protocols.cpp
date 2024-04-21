// SPDX-License-Identifier: MulanPSL-2.0

/*
 * vesper control socket 通信协议
 * 创建于 2024年3月6日 上海市嘉定区
 */

#include "./Protocols.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

using namespace std;

namespace vesper::control::protocol {

void Base::encode(stringstream& container) const {
    container.str(""); // clear content

    /* header */

    // magic

    container.write(MAGIC_STR, 4);

    // type

    uint32_t type = getType();
    auto typeBE = htobe32(type);
    container.write((char*) &typeBE, sizeof(type));

    // body length

    uint64_t len = bodyLength();
    auto lenBE = htobe64(len);
    container.write((char*) &lenBE, 8);

    /* body */

    encodeBody(container);
}

uint64_t Base::bodyLength() const {
    LOG_ERROR("protocol::Base::bodyLength called.");
    return 0;
}

void Base::encodeBody(stringstream&) const {
    LOG_ERROR("protocol::Base::encodeBody called.");
}

int Base::decodeBody(const char*, int) {
    return 0;
}

#define VESPER_CTRL_PROTO_IMPL_GET_TYPE(className) \
    uint32_t className::getType() const { return typeCode; }


VESPER_CTRL_PROTO_IMPL_GET_TYPE(Response);

uint64_t Response::bodyLength() const {
    return 8 + msg.length(); // 8B = 4B(code) + 4B(msg len)
}


void Response::encodeBody(stringstream& container) const {

    auto codeBE = htobe32(this->code);
    container.write((char*) &codeBE, sizeof(codeBE));

    auto len = uint32_t( msg.length() );
    auto lenBE = htobe32(len);
    container.write((char*) &lenBE, sizeof(lenBE));
    if (len > 0) {
        container.write(msg.data(), len);
    }
}

/* ------------ recv protocols ------------ */

VESPER_CTRL_EXPAND_RECV_PROTOS(VESPER_CTRL_PROTO_IMPL_GET_TYPE)



Base* decode(const char* data, uint32_t type, int len) {

    Base* p = nullptr;

#define TRY_MATCH(ProtoType) case ProtoType::typeCode: { p = new (nothrow) ProtoType; break; }

    switch (type) {
        VESPER_CTRL_EXPAND_RECV_PROTOS(TRY_MATCH)

        default: {
            LOG_WARN("type code ", type, "matches no protocols.");
            break;
        }
    }

#undef TRY_MATCH

    if (p && p->decodeBody(data + HEADER_LEN, len - HEADER_LEN)) {
        delete p;
        p = nullptr;
    }

    return p;
}

#undef VESPER_CTRL_PROTO_IMPL_GET_TYPE


} // namespace vesper::control::protocol
