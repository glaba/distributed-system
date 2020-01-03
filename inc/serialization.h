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

    void add_field(field const& f);
    auto serialize() const -> std::string;

    static void write_uint32_to_char_buf(uint32_t n, char *buf);

private:
    std::vector<field> fields;
};

class deserializer {
public:
    deserializer(char const* buf_, unsigned length_)
        : buf(buf_), length(length_), pos(0) {}

    auto get_int() -> uint32_t;
    auto get_string() -> std::string;
    void done();

    static auto read_uint32_from_char_buf(char const* buf) -> uint32_t;

private:
    char const* buf;
    unsigned length;
    unsigned pos;
};
