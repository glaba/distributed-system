#pragma once

#include "logging.h"
#include "member_list.h"

#include <string>
#include <iostream>

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>

typedef struct udp_client_info {
    int client_socket;
    socklen_t addr_len;
    struct sockaddr addr;
} udp_client_info;

using std::string;

class udp_server_intf {
public:
    virtual ~udp_server_intf() {}
    // Starts the server on the given port
    virtual void start_server(int port) = 0;
    // Stops the server
    virtual void stop_server() = 0;
    // Wrapper function around recvfrom that handles errors
    virtual int recv(char *buf, unsigned length) = 0;
};

class udp_server : public udp_server_intf {
public:
    udp_server(logger *lg_) : lg(lg_) {}
    ~udp_server() {}

    void start_server(int port);
    void stop_server();
    int recv(char *buf, unsigned length);
private:
    // Creates fd to receive incoming messages sent via UDP
    // Returns a socket fd, otherwise -1 on failure.
    int create_udp_server(int port);

    int server_fd;
    logger *lg;
};

class udp_client_intf {
public:
    virtual ~udp_client_intf() {}
    // Sends a UDP packet to the specified destination
    virtual void send(string host, int port, char *msg, unsigned length) = 0;
};

class udp_client : public udp_client_intf {
public:
    udp_client(logger *lg_) : lg(lg_) {}
    ~udp_client() {}

    void send(string host, int port, char *msg, unsigned length);
private:
    // Creates a UDP connection to a given host and port.
    // Returns a socket fd to the host, otherwise -1 on failure.
    udp_client_info create_udp_client(string host, string port);

    logger *lg;
};
