#pragma once

#include <memory>
#include <string>

class udp_server {
public:
    virtual ~udp_server() {}
    // Starts the server on the given port
    virtual void start_server(int port) = 0;
    // Stops the server
    virtual void stop_server() = 0;
    // Wrapper function around recvfrom that handles errors
    virtual int recv(char *buf, unsigned length) = 0;
};

class udp_client {
public:
    virtual ~udp_client() {}
    // Sends a UDP packet to the specified destination
    virtual void send(std::string host, int port, char *msg, unsigned length) = 0;
};

class udp_factory {
public:
    virtual std::unique_ptr<udp_client> get_udp_client() = 0;
    virtual std::unique_ptr<udp_server> get_udp_server() = 0;
};
