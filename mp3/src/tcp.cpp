#include "tcp.h"

#include <memory>

using std::unique_ptr;
using std::make_unique;

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

void tcp_server::setup_server(int port) {
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

void tcp_server::stop_server() {
    // Close socket and clear queue
    close(server_fd);
}

int tcp_server::accept_connection() {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        return -1;
    }

    return client_fd;
}

void tcp_server::close_connection(int client_socket) {
    // Close socket
    close(client_socket);
}

std::string tcp_server::read_from_client(int client) {
    ssize_t message_size;
    if ((message_size = tcp_utils::get_message_size(client)) == -1)
        return "";

    unique_ptr<char[]> buf = make_unique<char[]>(message_size);
    if (read_all_from_socket(client, buf.get(), message_size) == -1)
        return "";

    return std::string(buf.get(), message_size);
}

ssize_t tcp_server::write_to_client(int client, std::string data) {
    size_t size = (size_t) data.length();
    if (write_message_size(size, client) == -1) return -1;

    return write_all_to_socket(client, data.c_str(), data.length());
}

tcp_client::tcp_client(void) {}

int tcp_client::setup_connection(std::string host, int port) {
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

std::string tcp_client::read_from_server(int socket) {
    ssize_t message_size;
    if ((message_size = tcp_utils::get_message_size(socket)) == -1)
        return "";

    unique_ptr<char[]> buf = make_unique<char[]>(message_size);
    if (read_all_from_socket(socket, buf.get(), message_size) == -1)
        return "";

    return std::string(buf.get(), message_size);
}

ssize_t tcp_client::write_to_server(int socket, std::string data) {
    size_t size = (size_t) data.length();
    if (write_message_size(size, socket) == -1)
        return -1;

    return write_all_to_socket(socket, data.c_str(), data.length());
}

void tcp_client::close_connection(int socket) {
    // No internal state being managed so this is fine
    close(socket);
}
