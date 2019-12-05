#pragma once

#include <memory>

#define MAX_CLIENTS 10
#define CHUNK_SIZE 4096

class tcp_server {
public:
    virtual ~tcp_server() {}
    // Sets up server to receive connections on the given port
    virtual void setup_server(int port) = 0;
    // Stops the server
    virtual void stop_server() = 0;
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
    // Creates client connection to a server
    // Returns socket to server
    virtual int setup_connection(std::string host, int port) = 0;
    // Reads the specified number of bytes from the given socket
    // Returns data received from the server
    virtual std::string read_from_server(int socket) = 0;
    // Writes the specified number of bytes to the given socket
    // Returns number of bytes written, 0 on socket disconnect, and -1 on failure
    virtual ssize_t write_to_server(int socket, std::string data) = 0;
    // Cleans up the connection on the given socket
    virtual void close_connection(int socket) = 0;
};

class tcp_file_transfer {
public:
    // these utilities wrappers are so mocking can still work for file transfer
    static ssize_t write_file_to_socket(tcp_client *client, int socket, std::string filename);
    static ssize_t read_file_from_socket(tcp_client *client, int socket, std::string filename);
    static ssize_t write_file_to_socket(tcp_server *server, int socket, std::string filename);
    static ssize_t read_file_from_socket(tcp_server *server, int socket, std::string filename);
};

class tcp_factory {
public:
    virtual std::unique_ptr<tcp_client> get_tcp_client() = 0;
    virtual std::unique_ptr<tcp_server> get_tcp_server() = 0;
};
