#include "sdfs_utils.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>

int sdfs_utils::send_message(tcp_client *client, int socket, sdfs_message sdfs_msg) {
    std::string msg = sdfs_msg.serialize();
    if (client->write_to_server(socket, msg) == -1) return SDFS_FAILURE;
    return SDFS_SUCCESS;
}

int sdfs_utils::receive_message(tcp_client *client, int socket, sdfs_message *sdfs_msg) {
    std::string response;
    if ((response = client->read_from_server(socket)) == "") return SDFS_FAILURE;

    // determine if response was valid sdfs_message
    char response_cstr[response.length() + 1];
    strncpy(response_cstr, response.c_str(), response.length() + 1);
    *sdfs_msg = sdfs_message(response_cstr, strlen(response_cstr));
    if (sdfs_msg->get_type() == sdfs_message::msg_type::empty) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

int sdfs_utils::send_message(tcp_server *server, int socket, sdfs_message sdfs_msg) {
    std::string msg = sdfs_msg.serialize();
    if (server->write_to_client(socket, msg) == -1) return SDFS_FAILURE;
    return SDFS_SUCCESS;
}

int sdfs_utils::receive_message(tcp_server *server, int socket, sdfs_message *sdfs_msg) {
    std::string message;
    if ((message = server->read_from_client(socket)) == "") return SDFS_FAILURE;

    // determine if message was valid sdfs_message
    char message_cstr[message.length() + 1];
    strncpy(message_cstr, message.c_str(), message.length() + 1);
    *sdfs_msg = sdfs_message(message_cstr, strlen(message_cstr));
    if (sdfs_msg->get_type() == sdfs_message::msg_type::empty) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

/* @TODO: ADD THREAD-SAFE ACCESS VIA FLOCK TO ALL WRITES AND READS OF A FILE */

ssize_t sdfs_utils::write_file_to_socket(tcp_client *client, int socket, std::string filename) {
    int fd;
    struct stat fd_stats;

    if ((fd = open(filename.c_str(), O_RDONLY)) < 0) return -1;
    if (fstat(fd, &fd_stats) < 0) return -1;

    flock(fd, LOCK_SH);

    char *contents;
    if ((contents = (char *) mmap(0, fd_stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) ==
            (caddr_t) -1) {
        flock(fd, LOCK_UN);
        return -1;
    }

    // write total size of file to socket
    if ((client->write_to_server(socket, std::to_string(fd_stats.st_size))) == -1) {
        flock(fd, LOCK_UN);
        return -1;
    }

    size_t pos = 0;
    size_t num_to_write;
    ssize_t num_written;
    size_t bytes_left = fd_stats.st_size;
    while (bytes_left > 0) {
        // write file in CHUNK_SIZE chunks
        num_to_write = bytes_left > CHUNK_SIZE ? CHUNK_SIZE : bytes_left;
        std::string data(contents + pos, num_to_write);
        // virtual ssize_t write_to_server(int socket, std::string data) = 0;

        if ((num_written = client->write_to_server(socket, data)) == -1) {
            flock(fd, LOCK_UN);
            return -1;
        }

        pos += num_written;
        bytes_left -= num_written;
    }

    munmap(contents, fd_stats.st_size);
    flock(fd, LOCK_UN);

    return fd_stats.st_size;
}

ssize_t sdfs_utils::write_file_to_socket(tcp_server *server, int socket, std::string filename) {
    int fd;
    struct stat fd_stats;

    if ((fd = open(filename.c_str(), O_RDONLY)) < 0) return -1;
    if (fstat(fd, &fd_stats) < 0) return -1;

    flock(fd, LOCK_SH);

    char *contents;
    if ((contents = (char *) mmap(0, fd_stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) ==
            (caddr_t) -1) {
        flock(fd, LOCK_UN);
        return -1;
    }

    // write total size of file to socket
    if ((server->write_to_client(socket, std::to_string(fd_stats.st_size))) == -1) {
        flock(fd, LOCK_UN);
        return -1;
    }

    size_t pos = 0;
    size_t num_to_write;
    ssize_t num_written;
    size_t bytes_left = fd_stats.st_size;
    while (bytes_left > 0) {
        // write file in CHUNK_SIZE chunks
        num_to_write = bytes_left > CHUNK_SIZE ? CHUNK_SIZE : bytes_left;
        std::string data(contents + pos, num_to_write);
        // virtual ssize_t write_to_server(int socket, std::string data) = 0;

        if ((num_written = server->write_to_client(socket, data)) == -1) {
            flock(fd, LOCK_UN);
            return -1;
        }

        pos += num_written;
        bytes_left -= num_written;
    }

    munmap(contents, fd_stats.st_size);
    flock(fd, LOCK_UN);

    return fd_stats.st_size;
}

ssize_t sdfs_utils::read_file_from_socket(tcp_client *client, int socket, std::string filename) {
    int fd;

    if ((fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0x0777)) < 0) return -1;

    // read total size of file from socket
    std::string size_str;
    if ((size_str = client->read_from_server(socket)) == "") return -1;
    size_t size = std::stoul(size_str);

    // write byte at last location
    if (lseek(fd, size - 1, SEEK_SET) == -1) return -1;
    if (write(fd, "", 1) != 1) return -1;

    // mmap and read file from socket
    char *contents;
    if ((contents = (char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) ==
            (caddr_t) -1) return -1;

    size_t pos = 0;
    size_t num_read;
    size_t bytes_left = size;
    std::string data;
    while (bytes_left > 0) {
        // read file in CHUNK_SIZE chunks
        if ((data = client->read_from_server(socket)) == "") return -1;

        num_read = data.length();
        memcpy(contents + pos, data.c_str(), num_read);

        pos += num_read;
        bytes_left -= num_read;
    }

    munmap(contents, size);
    return size;
}

ssize_t sdfs_utils::read_file_from_socket(tcp_server *server, int socket, std::string filename) {
    int fd;

    if ((fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0x0777)) < 0) return -1;

    // read total size of file from socket
    std::string size_str;
    if ((size_str = server->read_from_client(socket)) == "") return -1;
    size_t size = std::stoul(size_str);

    // write byte at last location
    if (lseek(fd, size - 1, SEEK_SET) == -1) return -1;
    if (write(fd, "", 1) != 1) return -1;

    // mmap and read file from socket
    char *contents;
    if ((contents = (char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) ==
            (caddr_t) -1) return -1;

    size_t pos = 0;
    size_t num_read;
    size_t bytes_left = size;
    std::string data;
    while (bytes_left > 0) {
        // read file in CHUNK_SIZE chunks
        if ((data = server->read_from_client(socket)) == "") return -1;

        num_read = data.length();
        memcpy(contents + pos, data.c_str(), num_read);

        pos += num_read;
        bytes_left -= num_read;
    }

    munmap(contents, size);
    return size;
}
