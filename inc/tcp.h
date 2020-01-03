#pragma once

#include <memory>

#define MAX_CLIENTS 10
#define CHUNK_SIZE 4096

class tcp_server {
public:
    virtual ~tcp_server() {}
    // Accepts a connection on the server fd
    // Returns the socket to the accepted connection
    virtual auto accept_connection() -> int = 0;
    // Cleans up the connection on the given socket
    virtual void close_connection(int client_socket) = 0;
    // Reads the specified number of bytes from the given socket
    // Returns number of bytes read, 0 on socket disconnect, and -1 on failure
    virtual auto read_from_client(int client) -> std::string = 0;
    // Writes the specified number of bytes to the given socket
    // Returns number of bytes written, 0 on socket disconnect, and -1 on failure
    virtual auto write_to_client(int client, std::string const& data) -> ssize_t = 0;
};

class tcp_client {
public:
    virtual ~tcp_client() {}
    // Reads the specified number of bytes from the given socket
    // Returns data received from the server
    virtual auto read_from_server() -> std::string = 0;
    // Writes the specified number of bytes to the given socket
    // Returns number of bytes written, 0 on socket disconnect, and -1 on failure
    virtual auto write_to_server(std::string const& data) -> ssize_t = 0;
};

class tcp_factory {
public:
    virtual auto get_tcp_client(std::string const& host, int port) -> std::unique_ptr<tcp_client> = 0;
    virtual auto get_tcp_server(int port) -> std::unique_ptr<tcp_server> = 0;
};
