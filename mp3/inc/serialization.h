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
    std::unique_ptr<char[]> serialize(unsigned &length);

    static void write_uint32_to_char_buf(uint32_t n, char *buf);
private:

    std::vector<field> fields;
};

class deserializer {
public:
    deserializer(char *buf_, unsigned length_)
        : buf(buf_), length(length_), pos(0) {}

    uint32_t get_int();
    std::string get_string();
    void done();

    static uint32_t read_uint32_from_char_buf(char *buf);
private:
    char *buf;
    unsigned length;
    unsigned pos;
};
