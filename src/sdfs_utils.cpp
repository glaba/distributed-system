#include "sdfs_utils.hpp"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <iostream>

extern int errno;

using std::string;

auto sdfs_utils::send_message(tcp_client *client, sdfs_message const& sdfs_msg) -> int {
    string msg = sdfs_msg.serialize();
    if (client->write_to_server(msg) == -1) return SDFS_FAILURE;
    return SDFS_SUCCESS;
}

auto sdfs_utils::receive_message(tcp_client *client, sdfs_message *sdfs_msg) -> int {
    string message;
    if ((message = client->read_from_server()) == "") return SDFS_FAILURE;

    // determine if response was valid sdfs_message
    char message_cstr[message.length() + 1];
    memcpy(message_cstr, message.c_str(), message.length() + 1);
    *sdfs_msg = sdfs_message(message_cstr, message.length());
    if (sdfs_msg->get_type() == sdfs_message::msg_type::empty) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

auto sdfs_utils::send_message(tcp_server *server, int socket, sdfs_message const& sdfs_msg) -> int {
    string msg = sdfs_msg.serialize();
    if (server->write_to_client(socket, msg) == -1) return SDFS_FAILURE;
    return SDFS_SUCCESS;
}

auto sdfs_utils::receive_message(tcp_server *server, int socket, sdfs_message *sdfs_msg) -> int {
    string message;
    if ((message = server->read_from_client(socket)) == "") return SDFS_FAILURE;

    // determine if message was valid sdfs_message
    char message_cstr[message.length() + 1];
    memcpy(message_cstr, message.c_str(), message.length() + 1);
    *sdfs_msg = sdfs_message(message_cstr, message.length());
    if (sdfs_msg->get_type() == sdfs_message::msg_type::empty) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

/* @TODO: ADD THREAD-SAFE ACCESS VIA FLOCK TO ALL WRITES AND READS OF A FILE */

auto sdfs_utils::write_file_to_socket(tcp_client *client, string const& filename) -> ssize_t {
    int fd;

    if ((fd = open(filename.c_str(), O_RDONLY)) < 0) return -1;

    if (acquire_lock(fd, LOCK_SH) == -1) return -1;
    size_t filesize = lseek(fd, 0, SEEK_END);

    char *contents;
    if ((contents = (char *) mmap(0, filesize, PROT_READ, MAP_SHARED, fd, 0)) ==
            (caddr_t) -1) {
        release_lock(fd);
        return -1;
    }

    // write total size of file to socket
    if ((client->write_to_server(std::to_string(filesize))) == -1) {
        release_lock(fd);
        return -1;
    }

    size_t pos = 0;
    size_t num_to_write;
    ssize_t num_written;
    size_t bytes_left = filesize;
    while (bytes_left > 0) {
        // write file in CHUNK_SIZE chunks
        num_to_write = bytes_left > CHUNK_SIZE ? CHUNK_SIZE : bytes_left;
        string data(contents + pos, num_to_write);
        // virtual ssize_t write_to_server(int socket, string data) = 0;

        if ((num_written = client->write_to_server(data)) == -1) {
            release_lock(fd);
            return -1;
        }

        pos += num_written;
        bytes_left -= num_written;
    }

    munmap(contents, filesize);
    release_lock(fd);
    close(fd);

    return filesize;
}

auto sdfs_utils::write_file_to_socket(tcp_server *server, int socket, string const& filename) -> ssize_t {
    int fd;

    if ((fd = open(filename.c_str(), O_RDONLY)) < 0) return -1;

    if (acquire_lock(fd, LOCK_SH) == -1) return -1;
    size_t filesize = lseek(fd, 0, SEEK_END);

    char *contents;
    if ((contents = (char *) mmap(0, filesize, PROT_READ, MAP_SHARED, fd, 0)) ==
            (caddr_t) -1) {
        release_lock(fd);
        return -1;
    }

    // write total size of file to socket
    if ((server->write_to_client(socket, std::to_string(filesize))) == -1) {
        release_lock(fd);
        return -1;
    }

    size_t pos = 0;
    size_t num_to_write;
    ssize_t num_written;
    size_t bytes_left = filesize;
    while (bytes_left > 0) {
        // write file in CHUNK_SIZE chunks
        num_to_write = bytes_left > CHUNK_SIZE ? CHUNK_SIZE : bytes_left;
        string data(contents + pos, num_to_write);
        // virtual ssize_t write_to_server(int socket, string data) = 0;

        if ((num_written = server->write_to_client(socket, data)) == -1) {
            release_lock(fd);
            return -1;
        }

        pos += num_written;
        bytes_left -= num_written;
    }

    munmap(contents, filesize);
    release_lock(fd);
    close(fd);

    return filesize;
}

auto sdfs_utils::read_file_from_socket(tcp_client *client, string const& filename) -> ssize_t {
    int fd;

    if ((fd = open(filename.c_str(), O_RDWR | O_CREAT, (mode_t) 0644)) < 0) return -1;

    if (acquire_lock(fd, LOCK_EX) == -1) return -1;

    // read total size of file from socket
    string size_str;
    if ((size_str = client->read_from_server()) == "") {
        release_lock(fd);
        return -1;
    }

    size_t size = std::stoul(size_str);
    ftruncate(fd, size);

    // mmap and read file from socket
    char *contents;
    if ((contents = (char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
            (caddr_t) -1) {
        release_lock(fd);
        return -1;
    }

    size_t pos = 0;
    size_t num_read;
    size_t bytes_left = size;
    string data;
    while (bytes_left > 0) {
        // read file in CHUNK_SIZE chunks
        if ((data = client->read_from_server()) == "") {
            release_lock(fd);
            return -1;
        }

        num_read = data.length();
        memcpy(contents + pos, data.c_str(), num_read);

        pos += num_read;
        bytes_left -= num_read;
    }

    munmap(contents, size);
    release_lock(fd);
    close(fd);

    return size;
}

auto sdfs_utils::read_file_from_socket(tcp_server *server, int socket, string const& filename) -> ssize_t {
    int fd;

    if ((fd = open(filename.c_str(), O_RDWR | O_CREAT, (mode_t) 0644)) < 0) {
        return -1;
    }

    if (acquire_lock(fd, LOCK_EX) == -1) return -1;

    // read total size of file from socket
    string size_str;
    if ((size_str = server->read_from_client(socket)) == "") {
        release_lock(fd);
        return -1;
    }
    size_t size = std::stoul(size_str);
    ftruncate(fd, size);

    // mmap and read file from socket
    char *contents;
    if ((contents = (char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
            (caddr_t) -1) {
        release_lock(fd);
        return -1;
    }

    size_t pos = 0;
    size_t num_read;
    size_t bytes_left = size;
    string data;
    while (bytes_left > 0) {
        // read file in CHUNK_SIZE chunks
        if ((data = server->read_from_client(socket)) == "") {
            release_lock(fd);
            return -1;
        }

        num_read = data.length();
        memcpy(contents + pos, data.c_str(), num_read);

        pos += num_read;
        bytes_left -= num_read;
    }

    munmap(contents, size);
    release_lock(fd);
    close(fd);

    return size;
}

auto sdfs_utils::write_first_line_to_socket(tcp_server *server, int socket, string const& filename) -> ssize_t {
    int fd;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t num_bytes, num_written;

    if ((fp = fopen(filename.c_str(), "r")) == NULL) return -1;
    fd = fileno(fp);

    acquire_lock(fd, LOCK_SH);
    if ((num_bytes = getline(&line, &len, fp)) == -1) return -1;

    if ((num_written = server->write_to_client(socket, string(line))) == -1) {
        release_lock(fd);
        return -1;
    }

    if (line) free(line);
    release_lock(fd);
    fclose(fp);

    return num_written;
}


auto sdfs_utils::acquire_lock(int fd, int operation) -> int {
    while (flock(fd, operation) == -1) {
        // check errno
        if (errno != EINTR) return -1;
    }
    return 0;
}

auto sdfs_utils::release_lock(int fd) -> int {
    return flock(fd, LOCK_UN);
}
