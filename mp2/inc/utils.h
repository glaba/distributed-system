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

class udp_client_svc {
public:
	// Sends a UDP packet to the specified destination
	virtual void send(string host, string port, char *msg, unsigned length);
private:
	/*
	 * Creates a UDP connection to a given host and port.
	 * Returns a socket fd to the host, otherwise -1 on failure.
	 */
	udp_client_info udp_client(string host, string port);
};

class udp_server_svc {
public:
	// Starts the server on the given port
	virtual void start_server(int port);
	// Stops the server
	virtual void stop_server();
	// Wrapper function around recvfrom that handles errors
	virtual int recv(char *buf, unsigned length);
private:
	/*
	 * Creates fd to receive incoming messages sent via UDP
	 * Returns a socket fd, otherwise -1 on failure.
	 */
	int udp_server(int port);

	int server_fd;
};
