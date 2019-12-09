#pragma once

#include "sdfs_client.h"
#include "sdfs_message.h"
#include "sdfs_utils.hpp"
#include "election.h"
#include "logging.h"
#include "tcp.h"
#include "service.h"
#include "environment.h"

#include <sys/types.h>
#include <dirent.h>

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

    int put_operation(std::string local_filename, std::string sdfs_filename);
    int get_operation(std::string local_filename, std::string sdfs_filename);
    int del_operation(std::string sdfs_filename);
    int ls_operation(std::string sdfs_filename);
    int append_operation(std::string local_filename, std::string sdfs_filename);
    std::string get_metadata_operation(std::string sdfs_filename);

    int store_operation();
private:
    std::string put_operation_master(int socket, std::string local_filename, std::string sdfs_filename);
    std::string get_operation_master(int socket, std::string local_filename, std::string sdfs_filename);
    std::vector<std::string> append_operation_master(int socket, std::string metadata, std::string local_filename, std::string sdfs_filename);
    std::string get_metadata_operation_master(int socket, std::string sdfs_filename);
    int del_operation_master(int socket, std::string sdfs_filename);
    int ls_operation_master(int socket, std::string sdfs_filename);

    // int put_operation_internal(int socket, std::string local_filename, std::string sdfs_filename);
    int get_operation_internal(int socket, std::string local_filename, std::string sdfs_filename);
    std::string get_metadata_operation_internal(int socket, std::string sdfs_filename);
    int put_operation_internal(int socket, std::string local_filename, std::string sdfs_filename);

    int get_master_socket();
    int get_internal_socket(std::string hostname);

    // Services that we depend on
    election *el;
    std::unique_ptr<logger> lg;
    std::unique_ptr<tcp_client> client;
    std::unique_ptr<tcp_server> server;
    configuration *config;
};
