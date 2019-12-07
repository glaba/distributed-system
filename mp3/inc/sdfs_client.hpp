#pragma once

#include "sdfs_client.h"
#include "sdfs_message.h"
#include "sdfs_utils.hpp"
#include "election.h"
#include "logging.h"
#include "tcp.h"
#include "service.h"
#include "environment.h"

#include <string.h>

// Defining the return value for failed operations
#define SDFS_CLIENT_FAILURE -1
// Defining the return value for successful operations
#define SDFS_CLIENT_SUCCESS 0

class sdfs_client_impl : public sdfs_client, public service_impl<sdfs_client_impl> {
public:
    sdfs_client_impl(environment &env);

    void start();
    void stop();

    int put_operation(int socket, std::string local_filename, std::string sdfs_filename);
    int get_operation(int socket, std::string local_filename, std::string sdfs_filename);
    int del_operation(int socket, std::string sdfs_filename);
    int ls_operation(int socket, std::string sdfs_filename);
    int store_operation();
private:
    // Services that we depend on
    election *el;
    std::unique_ptr<logger> lg;
    std::unique_ptr<tcp_client> client;
    std::unique_ptr<tcp_server> server;
    configuration *config;
};
