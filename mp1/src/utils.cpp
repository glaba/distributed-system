#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "utils.h"
extern int errno;

#define MESSAGE_SIZE_DIGITS (sizeof(ssize_t))

ssize_t get_message_size(int socket) {
    int32_t size;
    ssize_t read_bytes =
        read_all_from_socket(socket, (char *)&size, MESSAGE_SIZE_DIGITS);
    if (read_bytes == 0 || read_bytes == -1)
        return read_bytes;

    return (ssize_t)ntohl(size);
}

ssize_t write_message_size(size_t size, int socket) {
    uint32_t nsize = htonl(size);
    ssize_t written_bytes =
        write_all_to_socket(socket, (char *)&nsize, MESSAGE_SIZE_DIGITS);
    if (written_bytes == 0 || written_bytes == -1)
        return written_bytes;

    return (ssize_t)size;
}

ssize_t read_all_from_socket(int socket, char *buffer, size_t count) {
    size_t total = 0;
    ssize_t r = 0;
    while (total < count && (r = read(socket, buffer + total, count - total))) {
        if (r == -1 && EINTR == errno) {
            continue;
        }

        if (r == -1) return -1;

        total += r;
    }

    return total;
}

ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
    size_t total = 0;
    ssize_t r = 0;
    while (total < count && (r = write(socket, buffer + total, count - total))) {
        if (r == -1 && EINTR == errno) {
            continue;
        }

        if (r == -1) return -1;

        total += r;
    }

    return total;
}
