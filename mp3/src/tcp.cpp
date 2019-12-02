#include "tcp.h"
#include "tcp.hpp"

#include <memory>

using std::unique_ptr;
using std::make_unique;

unique_ptr<tcp_client> tcp_factory_impl::get_tcp_client() {
    return unique_ptr<tcp_client>(new tcp_client_impl());
}

unique_ptr<tcp_server> tcp_factory_impl::get_tcp_server() {
    return unique_ptr<tcp_server>(new tcp_server_impl());
}

ssize_t tcp_utils::get_message_size(int socket) {
    int32_t size;
    ssize_t read_bytes =
        read_all_from_socket(socket, (char *) &size, sizeof(int32_t));
    if (read_bytes == 0 || read_bytes == -1)
        return read_bytes;

    return (ssize_t) ntohl(size);
}

ssize_t tcp_utils::write_message_size(size_t size, int socket) {
    uint32_t nsize = htonl(size);
    ssize_t written_bytes =
        write_all_to_socket(socket, (char *)&nsize, sizeof(uint32_t));
    if (written_bytes == 0 || written_bytes == -1)
        return written_bytes;

    return (ssize_t) size;
}

ssize_t tcp_utils::read_all_from_socket(int socket, char *buffer, size_t count) {
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

ssize_t tcp_utils::write_all_to_socket(int socket, const char *buffer, size_t count) {
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

ssize_t tcp_utils::write_file_to_socket(int socket, std::string filename) {
    int fd;
    struct stat fd_stats;

    if ((fd = open(filename.c_str(), O_RDONLY)) < 0) return -1;
    if (fstat(fd, &fd_stats) < 0) return -1;

    char *contents;
    if ((contents = (char *) mmap(0, fd_stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) ==
            (caddr_t) -1) return -1;

    // write total size of file to socket
    if ((write_message_size(socket, fd_stats.st_size)) == -1) return -1;

    size_t pos = 0;
    size_t num_to_write;
    ssize_t num_written;
    size_t bytes_left = fd_stats.st_size;
    while (bytes_left > 0) {
        // write file in CHUNK_SIZE chunks
        num_to_write = bytes_left > CHUNK_SIZE ? CHUNK_SIZE : bytes_left;
        if ((num_written = write_all_to_socket(socket,
                        contents + pos, num_to_write)) == -1) return -1;

        pos += num_written;
        bytes_left -= num_written;
    }

    munmap(contents, fd_stats.st_size);
    return fd_stats.st_size;
}

ssize_t tcp_utils::read_file_from_socket(int socket, std::string filename) {
    int fd;

    if ((fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0x0777)) < 0) return -1;

    // read total size of file from socket
    ssize_t size = get_message_size(socket);

    // write byte at last location
    if (lseek(fd, size - 1, SEEK_SET) == -1) return -1;
    if (write(fd, "", 1) != 1) return -1;

    // mmap and read file from socket
    char *contents;
    if ((contents = (char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) ==
            (caddr_t) -1) return -1;

    size_t pos = 0;
    size_t num_to_read;
    ssize_t num_read;
    size_t bytes_left = size;
    while (bytes_left > 0) {
        // read file in CHUNK_SIZE chunks
        num_to_read = bytes_left > CHUNK_SIZE ? CHUNK_SIZE : bytes_left;
        if ((num_read = read_all_from_socket(socket,
                        contents + pos, num_to_read)) == -1) return -1;

        pos += num_read;
        bytes_left -= num_read;
    }

    munmap(contents, size);
    return size;
}

void tcp_server_impl::setup_server(int port) {
    // Set up the server_fd
    struct addrinfo info, *res;
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&info, 0, sizeof(info));

    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;
    info.ai_flags = AI_PASSIVE;

    // getaddrinfo for the server port
    int s = getaddrinfo(NULL, std::to_string(port).c_str(), &info, &res);
    if (s != 0) {
        // Get the error using gai
        std::cerr << "gai failed " << gai_strerror(s) << std::endl;
        free(res);
        exit(1);
    }

    // Bind server socket to port and address
    if (bind(fd, res->ai_addr, res->ai_addrlen) != 0) {
        perror("bind failed");
        free(res);
        exit(1);
    }
    free(res);

    // Set up listening for clients
    if (listen(fd, MAX_CLIENTS) != 0) {
        perror("listen failed");
        exit(1);
    }

    server_fd = fd;
}

void tcp_server_impl::stop_server() {
    // Close socket and clear queue
    close(server_fd);
}

int tcp_server_impl::accept_connection() {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        return -1;
    }

    return client_fd;
}

void tcp_server_impl::close_connection(int client_socket) {
    // Close socket
    close(client_socket);
}

std::string tcp_server_impl::read_from_client(int client) {
    ssize_t message_size;
    if ((message_size = tcp_utils::get_message_size(client)) == -1)
        return "";

    unique_ptr<char[]> buf = make_unique<char[]>(message_size);
    if (read_all_from_socket(client, buf.get(), message_size) == -1)
        return "";

    return std::string(buf.get(), message_size);
}

ssize_t tcp_server_impl::write_to_client(int client, std::string data) {
    size_t size = (size_t) data.length();
    if (write_message_size(size, client) == -1) return -1;

    return write_all_to_socket(client, data.c_str(), data.length());
}

tcp_client_impl::tcp_client_impl(void) {}

int tcp_client_impl::setup_connection(std::string host, int port) {
    struct addrinfo info, *res;
    memset(&info, 0, sizeof(info));

    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &info, &res);
    if (s != 0) {
        // Get the error using gai
        std::cerr << "gai failed " << gai_strerror(s) << std::endl;
        return -1;
    }

    // Get a socket for the client
    int client_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_socket == -1) {
        perror("socket failed");
        return -1;
    }

    // Connect the client to the server
    int connected = connect(client_socket, res->ai_addr, res->ai_addrlen);
    if (connected == -1) {
        perror("connect failed");
        return -1;
    }

    return client_socket;
}

std::string tcp_client_impl::read_from_server(int socket) {
    ssize_t message_size;
    if ((message_size = tcp_utils::get_message_size(socket)) == -1)
        return "";

    unique_ptr<char[]> buf = make_unique<char[]>(message_size);
    if (read_all_from_socket(socket, buf.get(), message_size) == -1)
        return "";

    return std::string(buf.get(), message_size);
}

ssize_t tcp_client_impl::write_to_server(int socket, std::string data) {
    size_t size = (size_t) data.length();
    if (write_message_size(size, socket) == -1)
        return -1;

    return write_all_to_socket(socket, data.c_str(), data.length());
}

void tcp_client_impl::close_connection(int socket) {
    // No internal state being managed so this is fine
    close(socket);
}

register_service<tcp_factory, tcp_factory_impl> register_tcp_factory;
