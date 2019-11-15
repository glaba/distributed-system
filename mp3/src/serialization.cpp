#include "serialization.h"

#include <cstring>
#include <cassert>
#include <iostream>

using std::unique_ptr;
using std::make_unique;

void serializer::add_field(field f) {
    fields.push_back(f);
}

unique_ptr<char[]> serializer::serialize(unsigned &length) {
    // First, compute the length
    length = 0;
    for (field f : fields) {
        if (std::holds_alternative<uint32_t>(f)) {
            length += sizeof(uint32_t);
        }

        if (std::holds_alternative<std::string>(f)) {
            length += sizeof(uint32_t) + std::get<std::string>(f).size();
        }
    }

    // Create the buffer to contain the serialized data
    unique_ptr<char[]> buf = make_unique<char[]>(length);

    // Loop through all the fields and insert them into the buffer
    unsigned pos = 0;
    for (field f : fields) {
        if (std::holds_alternative<uint32_t>(f)) {
            write_uint32_to_char_buf(std::get<uint32_t>(f), buf.get() + pos);
            pos += sizeof(uint32_t);
        }

        if (std::holds_alternative<std::string>(f)) {
            std::string str = std::get<std::string>(f);

            write_uint32_to_char_buf(str.size(), buf.get() + pos);
            pos += sizeof(uint32_t);

            std::strncpy(buf.get() + pos, str.c_str(), str.size());
            pos += str.size();
        }
    }

    assert(pos == length && "Position does not match length of buffer and memory corruption may have occurred!");

    return buf;
}

void serializer::write_uint32_to_char_buf(uint32_t n, char *buf) {
    buf[0] = (n & (0xFF << 0)) >> 0;
    buf[1] = (n & (0xFF << 8)) >> 8;
    buf[2] = (n & (0xFF << 16)) >> 16;
    buf[3] = (n & (0xFF << 24)) >> 24;
}

uint32_t deserializer::get_int() {
    if (length - pos < sizeof(uint32_t)) {
        throw "Could not extract uint32_t from buffer";
    }

    uint32_t val = read_uint32_from_char_buf(buf + pos);
    pos += sizeof(uint32_t);

    return val;
}

std::string deserializer::get_string() {
    if (length - pos < sizeof(uint32_t)) {
        throw "Could not extract string from buffer";
    }

    uint32_t size = read_uint32_from_char_buf(buf + pos);
    pos += sizeof(uint32_t);

    if (size > MAX_DESERIALIZABLE_STRING_LEN) {
        throw "Could not extract string from buffer";
    }

    // Make sure there are that many bytes left in the buffer
    if (length - pos < size) {
        throw "Could not extract string from buffer";
    }

    unique_ptr<char[]> str_buf = make_unique<char[]>(size);
    std::strncpy(str_buf.get(), buf + pos, size);
    pos += size;

    return std::string(str_buf.get(), size);
}

void deserializer::done() {
    if (pos != length) {
        throw "Did not fully consume message";
    }
}

uint32_t deserializer::read_uint32_from_char_buf(char *buf) {
    uint32_t retval = 0;
    retval += (buf[0] & 0xFF);
    retval += (buf[1] & 0xFF) << 8;
    retval += (buf[2] & 0xFF) << 16;
    retval += (buf[3] & 0xFF) << 24;
    return retval;
}
