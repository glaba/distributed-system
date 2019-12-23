#pragma once

#include <memory>

#define MAX_CLIENTS 10
#define CHUNK_SIZE 4096

class tcp_server {
public:
    virtual ~tcp_server() {}
    // Accepts a connection on the server fd
    // Returns the socket to the accepted connection
    virtual int accept_connection() = 0;
    // Cleans up the connection on the given socket
    virtual void close_connection(int client_socket) = 0;
    // Reads the specified number of bytes from the given socket
    // Returns number of bytes read, 0 on socket disconnect, and -1 on failure
    virtual std::string read_from_client(int client) = 0;
    // Writes the specified number of bytes to the given socket
    // Returns number of bytes written, 0 on socket disconnect, and -1 on failure
    virtual ssize_t write_to_client(int client, std::string data) = 0;
};

class tcp_client {
public:
    virtual ~tcp_client() {}
    // Reads the specified number of bytes from the given socket
    // Returns data received from the server
    virtual std::string read_from_server() = 0;
    // Writes the specified number of bytes to the given socket
    // Returns number of bytes written, 0 on socket disconnect, and -1 on failure
    virtual ssize_t write_to_server(std::string data) = 0;
};

class tcp_factory {
public:
    virtual std::unique_ptr<tcp_client> get_tcp_client(std::string host, int port) = 0;
    virtual std::unique_ptr<tcp_server> get_tcp_server(int port) = 0;
};
