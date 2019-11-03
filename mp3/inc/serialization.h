#pragma once

#include <cstdint>

class serialization {
public:
    static uint32_t read_uint32_from_char_buf(char *buf);
    static void write_uint32_to_char_buf(uint32_t n, char *buf);
};
