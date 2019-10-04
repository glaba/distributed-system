#pragma once
#include <string>

using std::string;

/*
 * Creates a UDP connection to a given host and port.
 * Returns a socket fd to the host, otherwise -1 on failure.
 */
int udp_client(string host, string port);

/*
 * Creates fd to receive incoming messages sent via UDP
 * Returns a socket fd, otherwise -1 on failure.
 */
int udp_server();
