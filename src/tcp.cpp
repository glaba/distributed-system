#include "tcp.h"
#include "tcp.hpp"

#include <memory>
#include <signal.h>

using std::string;
using std::unique_ptr;
using std::make_unique;

void signal_handler(int signum) {}

tcp_factory_impl::tcp_factory_impl(environment &env)
    : lg_fac(env.get<logger_factory>())
{
    signal(SIGPIPE, signal_handler);
}

auto tcp_factory_impl::get_tcp_client(string const& host, int port) -> unique_ptr<tcp_client> {
    tcp_client_impl *retval = new tcp_client_impl(lg_fac->get_logger("tcp_client"));
    if (retval->setup_connection(host, port) < 0) {
        return nullptr;
    } else {
        return unique_ptr<tcp_client>(static_cast<tcp_client*>(retval));
    }
}

auto tcp_factory_impl::get_tcp_server(int port) -> unique_ptr<tcp_server> {
    tcp_server_impl *retval = new tcp_server_impl(lg_fac->get_logger("tcp_server"));
    retval->setup_server(port);
    return unique_ptr<tcp_server>(static_cast<tcp_server*>(retval));
}

auto tcp_utils::get_message_size(int socket) -> ssize_t {
    int32_t size;
    ssize_t read_bytes =
        read_all_from_socket(socket, (char *) &size, sizeof(int32_t));
    if (read_bytes == 0 || read_bytes == -1)
        return read_bytes;

    return (ssize_t) ntohl(size);
}

auto tcp_utils::write_message_size(size_t size, int socket) -> ssize_t {
    uint32_t nsize = htonl(size);
    ssize_t written_bytes =
        write_all_to_socket(socket, (char *)&nsize, sizeof(uint32_t));
    if (written_bytes == 0 || written_bytes == -1)
        return written_bytes;

    return (ssize_t) size;
}

auto tcp_utils::read_all_from_socket(int socket, char *buffer, size_t count) -> ssize_t {
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

auto tcp_utils::write_all_to_socket(int socket, const char *buffer, size_t count) -> ssize_t {
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

tcp_server_impl::~tcp_server_impl() {
    stop_server();
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
        lg->info("getaddrinfo failed -- " + string(gai_strerror(s)));
        free(res);
        exit(1);
    }

    // Bind server socket to port and address
    if (bind(fd, res->ai_addr, res->ai_addrlen) != 0) {
        lg->info("bind failed -- " + string(strerror(errno)));
        free(res);
        exit(1);
    }
    free(res);

    // Set up listening for clients
    if (listen(fd, MAX_CLIENTS) != 0) {
        lg->info("listen failed -- " + string(strerror(errno)));
        exit(1);
    }

    server_fd = fd;
}

void tcp_server_impl::stop_server() {
    // Close socket
    shutdown(server_fd, SHUT_RDWR);
    close(server_fd);
}

auto tcp_server_impl::accept_connection() -> int {
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

auto tcp_server_impl::read_from_client(int client) -> string {
    ssize_t message_size;
    if ((message_size = get_message_size(client)) == -1)
        return "";

    unique_ptr<char[]> buf = make_unique<char[]>(message_size);
    if (read_all_from_socket(client, buf.get(), message_size) == -1)
        return "";

    return string(buf.get(), message_size);
}

auto tcp_server_impl::write_to_client(int client, string const& data) -> ssize_t {
    size_t size = (size_t) data.length();
    if (write_message_size(size, client) == -1) return -1;

    return write_all_to_socket(client, data.c_str(), data.length());
}

tcp_client_impl::~tcp_client_impl() {
    close_connection();
}

auto tcp_client_impl::setup_connection(string const& host, int port) -> int {
    struct addrinfo info, *res;
    memset(&info, 0, sizeof(info));

    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &info, &res);
    if (s != 0) {
        // Get the error using gai
        lg->info("getaddrinfo failed -- " + string(gai_strerror(s)));
        fixed_socket = -1;
        return fixed_socket;
    }

    // Get a socket for the client
    int client_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_socket == -1) {
        lg->info("socket failed -- " + string(strerror(errno)));
        fixed_socket = -1;
        return fixed_socket;
    }

    // Connect the client to the server
    int connected = connect(client_socket, res->ai_addr, res->ai_addrlen);
    if (connected == -1) {
        lg->info("connect failed -- " + string(strerror(errno)));
        fixed_socket = -1;
        return fixed_socket;
    }

    fixed_socket = client_socket;
    return fixed_socket;
}

auto tcp_client_impl::read_from_server() -> string {
    ssize_t message_size;
    if ((message_size = get_message_size(fixed_socket)) == -1)
        return "";

    unique_ptr<char[]> buf = make_unique<char[]>(message_size);
    if (read_all_from_socket(fixed_socket, buf.get(), message_size) == -1)
        return "";

    return string(buf.get(), message_size);
}

auto tcp_client_impl::write_to_server(string const& data) -> ssize_t {
    size_t size = (size_t) data.length();
    if (write_message_size(size, fixed_socket) == -1)
        return -1;

    return write_all_to_socket(fixed_socket, data.c_str(), data.length());
}

void tcp_client_impl::close_connection() {
    // No internal state being managed so this is fine
    close(fixed_socket);
}

register_service<tcp_factory, tcp_factory_impl> register_tcp_factory;
