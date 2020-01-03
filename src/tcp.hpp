#pragma once

#include "tcp.h"
#include "environment.h"
#include "service.h"
#include "logging.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <queue>
#include <string>
#include <iostream>
#include <memory>

class tcp_utils {
public:
    // Read the first four bytes from socket and transform it into ssize_t
    // Returns the size of the incomming message, 0 if socket is disconnected, -1 on failure
    auto get_message_size(int socket) -> ssize_t;

    // Writes the bytes of size to the socket
    // Returns the number of bytes successfully written, 0 if socket is disconnected, or -1 on failure
    auto write_message_size(size_t size, int socket) -> ssize_t;

    // Attempts to read all count bytes from socket into buffer.
    // Returns the number of bytes read, 0 if socket is disconnected, or -1 on failure.
    auto read_all_from_socket(int socket, char *buffer, size_t count) -> ssize_t;

    // Attempts to write all count bytes from buffer to socket.
    // Returns the number of bytes written, 0 if socket is disconnected, or -1 on failure.
    auto write_all_to_socket(int socket, const char *buffer, size_t count) -> ssize_t;

    // Attempts to write entire file over socket
    // Returns the number of bytes written
    auto write_file_to_socket(int socket, std::string filename) -> ssize_t;

    // Attempts to read entire file over socket
    // Returns the number of bytes read
    auto read_file_from_socket(int socket, std::string filename) -> ssize_t;
};

class tcp_server_impl : public tcp_server, public tcp_utils {
public:
    tcp_server_impl(std::unique_ptr<logger> lg_): lg(std::move(lg_)) {}
    ~tcp_server_impl();
    void setup_server(int port);
    void stop_server();
    auto accept_connection() -> int;
    void close_connection(int client_socket);
    auto read_from_client(int client) -> std::string;
    auto write_to_client(int client, std::string const& data) -> ssize_t;
private:
    std::unique_ptr<logger> lg;
    int server_fd;
    std::queue<std::string> messages;
};

class tcp_client_impl : public tcp_client, public tcp_utils {
public:
    tcp_client_impl(std::unique_ptr<logger> lg_) : lg(std::move(lg_)) {}
    ~tcp_client_impl();
    auto setup_connection(std::string const& host, int port) -> int;
    void close_connection();
    auto read_from_server() -> std::string;
    auto write_to_server(std::string const& data) -> ssize_t;
private:
    std::unique_ptr<logger> lg;
    int fixed_socket;
};

class tcp_factory_impl : public tcp_factory, public service_impl<tcp_factory_impl> {
public:
    tcp_factory_impl(environment &env);

    auto get_tcp_client(std::string const& host, int port) -> std::unique_ptr<tcp_client>;
    auto get_tcp_server(int port) -> std::unique_ptr<tcp_server>;

private:
    logger_factory *lg_fac;
};