#pragma once

#include "sdfs_server.h"
#include "sdfs_message.h"
#include "election.h"
#include "logging.h"
#include "tcp.h"
#include "service.h"
#include "environment.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Defining the return value for failed operations
#define SDFS_SERVER_FAILURE -1
// Defining the return value for successful operations
#define SDFS_SERVER_SUCCESS 0

class sdfs_server_impl : public sdfs_server, public service_impl<sdfs_server_impl> {
public:
    sdfs_server_impl(environment &env);

    void start();
    void stop();

private:
    // functions to handle major sdfs operations
    int put_operation(int socket, std::string sdfs_filename);
    int get_operation(int socket, std::string sdfs_filename);
    int del_operation(int socket, std::string sdfs_filename);
    int ls_operation(int socket, std::string sdfs_filename);

    int send_client_ack(int socket);

    int del_file(std::string sdfs_filename);
    bool file_exists(std::string sdfs_filename);

    // Services that we depend on
    std::unique_ptr<logger> lg;
    std::unique_ptr<tcp_client> client;
    std::unique_ptr<tcp_server> server;
    configuration *config;
};
