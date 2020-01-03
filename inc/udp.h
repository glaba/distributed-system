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
    virtual auto recv(char *buf, unsigned length) -> int = 0;
};

class udp_client {
public:
    virtual ~udp_client() {}
    // Sends a UDP packet to the specified destination
    virtual void send(std::string const& host, int port, std::string const& msg) = 0;
};

class udp_factory {
public:
    virtual auto get_udp_client() -> std::unique_ptr<udp_client> = 0;
    virtual auto get_udp_server() -> std::unique_ptr<udp_server> = 0;
};
