#pragma once
#include <string>
#include <iostream>

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef struct udp_client_info {
    int client_socket;
    socklen_t addr_len;
    struct sockaddr addr;
} udp_client_info;

using std::string;

/*
 * Creates a UDP connection to a given host and port.
 * Returns a socket fd to the host, otherwise -1 on failure.
 */
udp_client_info udp_client(string host, string port);

/*
 * Creates fd to receive incoming messages sent via UDP
 * Returns a socket fd, otherwise -1 on failure.
 */
int udp_server(int port);
