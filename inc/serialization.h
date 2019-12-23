#pragma once

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>
#include <string>

#define MAX_DESERIALIZABLE_STRING_LEN 1024

class serializer {
public:
    using field = std::variant<uint32_t, std::string>;

    void add_field(field f);
    std::string serialize();

    static void write_uint32_to_char_buf(uint32_t n, char *buf);
private:

    std::vector<field> fields;
};

class deserializer {
public:
    deserializer(const char *buf_, unsigned length_)
        : buf(buf_), length(length_), pos(0) {}

    uint32_t get_int();
    std::string get_string();
    void done();

    static uint32_t read_uint32_from_char_buf(const char *buf);
private:
    const char *buf;
    unsigned length;
    unsigned pos;
};
