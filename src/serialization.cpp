#include "serialization.h"
#include "overloaded.h"

#include <cstring>
#include <cassert>
#include <iostream>

using std::string;
using std::unique_ptr;
using std::make_unique;

namespace serialization {
    void write_uint32_to_char_buf(uint32_t n, char *buf) {
        buf[0] = (n & (0xFF << 0)) >> 0;
        buf[1] = (n & (0xFF << 8)) >> 8;
        buf[2] = (n & (0xFF << 16)) >> 16;
        buf[3] = (n & (0xFF << 24)) >> 24;
    }

    auto read_uint32_from_char_buf(char const* buf) -> uint32_t {
        uint32_t retval = 0;
        retval += (buf[0] & 0xFF);
        retval += (buf[1] & 0xFF) << 8;
        retval += (buf[2] & 0xFF) << 16;
        retval += (buf[3] & 0xFF) << 24;
        return retval;
    }
}

void serializer::add_field(field const& f) {
    fields.push_back(f);
}

auto serializer::serialize() const -> string {
    // First, compute the length
    unsigned length = 0;
    for (field f : fields) {
        if (std::holds_alternative<uint32_t>(f)) {
            length += sizeof(uint32_t);
        }

        if (std::holds_alternative<string>(f)) {
            length += sizeof(uint32_t) + std::get<string>(f).size();
        }
    }

    // Create the buffer to contain the serialized data
    unique_ptr<char[]> buf = make_unique<char[]>(length);

    // Loop through all the fields and insert them into the buffer
    unsigned pos = 0;
    for (field f : fields) {
        if (std::holds_alternative<uint32_t>(f)) {
            serialization::write_uint32_to_char_buf(std::get<uint32_t>(f), buf.get() + pos);
            pos += sizeof(uint32_t);
        }

        if (std::holds_alternative<string>(f)) {
            string str = std::get<string>(f);

            serialization::write_uint32_to_char_buf(str.size(), buf.get() + pos);
            pos += sizeof(uint32_t);

            std::memcpy(buf.get() + pos, str.c_str(), str.size());
            pos += str.size();
        }
    }

    return string(buf.get(), length);
}

deserializer::deserializer(char const* buf_, unsigned length_)
    : buf(buf_), length(length_), pos(0) {}

auto deserializer::get_int() -> uint32_t {
    if (length - pos < sizeof(uint32_t)) {
        throw "Could not extract uint32_t from buffer";
    }

    uint32_t val = serialization::read_uint32_from_char_buf(buf + pos);
    pos += sizeof(uint32_t);

    return val;
}

auto deserializer::get_string() -> string {
    if (length - pos < sizeof(uint32_t)) {
        throw "Could not extract string from buffer";
    }

    uint32_t size = serialization::read_uint32_from_char_buf(buf + pos);
    pos += sizeof(uint32_t);

    if (size > MAX_DESERIALIZABLE_STRING_LEN) {
        throw "Could not extract string from buffer";
    }

    // Make sure there are that many bytes left in the buffer
    if (length - pos < size) {
        throw "Could not extract string from buffer";
    }

    unique_ptr<char[]> str_buf = make_unique<char[]>(size);
    std::memcpy(str_buf.get(), buf + pos, size);
    pos += size;

    return string(str_buf.get(), size);
}

void deserializer::done() {
    if (pos != length) {
        throw "Did not fully consume message";
    }
}
