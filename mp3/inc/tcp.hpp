#pragma once

#include "tcp.h"
#include "environment.h"
#include "service.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
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
    ssize_t get_message_size(int socket);

    // Writes the bytes of size to the socket
    // Returns the number of bytes successfully written, 0 if socket is disconnected, or -1 on failure
    ssize_t write_message_size(size_t size, int socket);

    // Attempts to read all count bytes from socket into buffer.
    // Returns the number of bytes read, 0 if socket is disconnected, or -1 on failure.
    ssize_t read_all_from_socket(int socket, char *buffer, size_t count);

    // Attempts to write all count bytes from buffer to socket.
    // Returns the number of bytes written, 0 if socket is disconnected, or -1 on failure.
    ssize_t write_all_to_socket(int socket, const char *buffer, size_t count);
};

class tcp_server_impl : public tcp_server, public tcp_utils {
public:
    void setup_server(int port);
    void stop_server();
    int accept_connection();
    void close_connection(int client_socket);
    std::string read_from_client(int client);
    ssize_t write_to_client(int client, std::string data);
private:
    int server_fd;
    std::queue<std::string> messages;
};

class tcp_client_impl : public tcp_client, public tcp_utils {
public:
    tcp_client_impl(void);
    int setup_connection(std::string host, int port);
    void close_connection(int socket);
    std::string read_from_server(int socket);
    ssize_t write_to_server(int socket, std::string data);
};

class tcp_factory_impl : public tcp_factory, public service_impl<tcp_factory_impl> {
public:
    tcp_factory_impl(environment &env) {}

    std::unique_ptr<tcp_client> get_tcp_client();
    std::unique_ptr<tcp_server> get_tcp_server();
};