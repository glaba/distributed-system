#pragma once

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <string>

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

class tcp_server_intf : tcp_utils {
public:
    // Sets up server to receive connections on the given port
    virtual void setup_server(int port) = 0;
    // Tears down to server
    virtual void tear_down_server() = 0;
    // Accepts a connection on the server fd
    // Returns the socket to the accepted connection
    virtual int accept_connection() = 0;
    // Cleans up the connection on the given socket
    virtual void close_connection(int client_socket) = 0;
    // Reads the specified number of bytes from the given socket
    // Returns number of bytes read, 0 on socket disconnect, and -1 on failure
    virtual ssize_t read_from_client(int client, char *buffer, size_t count) = 0;
    // Writes the specified number of bytes to the given socket
    // Returns number of bytes written, 0 on socket disconnect, and -1 on failure
    virtual ssize_t write_to_client(int client, const char *buffer, size_t count) = 0;
};

class tcp_server : public tcp_server_intf {
public:
    void setup_server(int port);
    void tear_down_server();
    int accept_connection();
    void close_connection(int client_socket);
    ssize_t read_from_client(int client, char *buffer, size_t count);
    ssize_t write_to_client(int client, const char *buffer, size_t count);
private:
    int server_fd;
};

class tcp_client_intf : tcp_utils {
public:
    // Creates client connection to a server
    // Returns socket to server
    virtual int setup_connection(std::string host, std::string port) = 0;
    // Reads the specified number of bytes from the given socket
    // Returns number of bytes read, 0 on socket disconnect, and -1 on failure
    virtual ssize_t read_from_server(int socket, char *buffer, size_t count) = 0;
    // Writes the specified number of bytes to the given socket
    // Returns number of bytes written, 0 on socket disconnect, and -1 on failure
    virtual ssize_t write_to_server(int socket, const char *buffer, size_t count) = 0;
    // Cleans up the connection on the given socket
    virtual void close_connection(int socket) = 0;
};

class tcp_client : public tcp_client_intf {
public:
    int setup_connection(std::string host, std::string port);
    ssize_t read_from_server(int socket, char *buffer, size_t count);
    ssize_t write_to_server(int socket, const char *buffer, size_t count);
    void close_connection(int socket);
};
