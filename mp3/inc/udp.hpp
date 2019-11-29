#pragma once

#include "udp.h"
#include "logging.h"
#include "member_list.h"
#include "environment.h"

#include <string>
#include <iostream>
#include <memory>
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

class udp_server_impl : public udp_server {
public:
    udp_server_impl(environment &env) : lg(env.get<logger_factory>()->get_logger("udp_server")) {}
    ~udp_server_impl() {}

    void start_server(int port);
    void stop_server();
    int recv(char *buf, unsigned length);
private:
    // Creates fd to receive incoming messages sent via UDP
    // Returns a socket fd, otherwise -1 on failure.
    int create_udp_server(int port);

    int server_fd;
    std::unique_ptr<logger> lg;
};

class udp_client_impl : public udp_client {
public:
    udp_client_impl(environment &env) : lg(env.get<logger_factory>()->get_logger("udp_client")) {}
    ~udp_client_impl() {}

    void send(string host, int port, char *msg, unsigned length);
private:
    // Creates a UDP connection to a given host and port.
    // Returns a socket fd to the host, otherwise -1 on failure.
    udp_client_info create_udp_client(string host, string port);

    std::unique_ptr<logger> lg;
};

class udp_factory_impl : public udp_factory, public service_impl<udp_factory_impl> {
public:
    udp_factory_impl(environment &env_)
        : env(env_) {}

    std::unique_ptr<udp_client> get_udp_client() {
        return std::unique_ptr<udp_client>(new udp_client_impl(env));
    }
    std::unique_ptr<udp_server> get_udp_server() {
        return std::unique_ptr<udp_server>(new udp_server_impl(env));
    }

private:
    environment &env;
};
