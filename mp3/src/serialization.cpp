#include "serialization.h"

uint32_t serialization::read_uint32_from_char_buf(char *buf) {
    uint32_t retval = 0;
    retval += (buf[0] & 0xFF);
    retval += (buf[1] & 0xFF) << 8;
    retval += (buf[2] & 0xFF) << 16;
    retval += (buf[3] & 0xFF) << 24;
    return retval;
}

void serialization::write_uint32_to_char_buf(uint32_t n, char *buf) {
    buf[0] = (n & (0xFF << 0)) >> 0;
    buf[1] = (n & (0xFF << 8)) >> 8;
    buf[2] = (n & (0xFF << 16)) >> 16;
    buf[3] = (n & (0xFF << 24)) >> 24;
}