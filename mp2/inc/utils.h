#pragma once
#include <string>

using std::string;

/*
    creates a UDP connection to a given host and port.
    returns a socket fd to the host, otherwise -1 on failure.
 */
int udp_client(string host, string port);

/*
   creates fd to receive incoming messages sent via UDP
   returns a socket fd, otherwise -1 on failure.
 */
int udp_server();
