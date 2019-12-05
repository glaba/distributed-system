#pragma once

#include "sdfs_master.h"
#include "sdfs_message.h"
#include "election.h"
#include "logging.h"
#include "tcp.h"
#include "service.h"
#include "environment.h"

#include <string>
#include <cstring>
#include <unordered_map>

// Defining the return value for failed operations
#define SDFS_MASTER_FAILURE -1
// Defining the return value for successful operations
#define SDFS_MASTER_SUCCESS 0

class sdfs_master_impl : public sdfs_master, public service_impl<sdfs_master_impl> {
public:
    sdfs_master_impl(environment &env);

    void start();
    void stop();

private:
    // Functions to handle major sdfs operations as the master
    int put_operation(int socket, std::string sdfs_filename);
    int get_operation(int socket, std::string sdfs_filename);
    int del_operation(int socket, std::string sdfs_filename);
    int ls_operation(int socket, std::string sdfs_filename);

    int send_message(int socket, sdfs_message sdfs_msg);
    int receive_message(int socket, sdfs_message *sdfs_msg);

    bool sdfs_file_exists(std::string sdfs_filename);

    // Services that we depend on
    election *el;
    std::unique_ptr<logger> lg;
    std::unique_ptr<tcp_client> client;
    std::unique_ptr<tcp_server> server;
    std::unique_ptr<std::unordered_map<std::string, std::vector<std::string>>> file_to_hostnames;
    std::unique_ptr<std::unordered_map<std::string, std::vector<std::string>>> hostname_to_files;
    configuration *config;
};
