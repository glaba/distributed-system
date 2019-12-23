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
#include <random>
#include <stdlib.h>
#include <memory>

// Defining the return value for failed operations
#define SDFS_CLIENT_FAILURE -1
// Defining the return value for successful operations
#define SDFS_CLIENT_SUCCESS 0

class sdfs_client_impl : public sdfs_client, public service_impl<sdfs_client_impl> {
public:
    sdfs_client_impl(environment &env);

    void start();
    void stop();

    void set_master_node(std::string hostname) {
        mn_hostname = hostname;
    };
    int put_operation(std::string local_filename, std::string sdfs_filename);
    int put_operation(inputter<std::string> in, std::string sdfs_filename) {return -1;}
    int get_operation(std::string local_filename, std::string sdfs_filename);
    int del_operation(std::string sdfs_filename);
    int ls_operation(std::string sdfs_filename);
    int append_operation(std::string local_filename, std::string sdfs_filename);
    int append_operation(inputter<std::string> in, std::string sdfs_filename) {return -1;}
    int get_index_operation(std::string sdfs_filename);
    std::string get_metadata_operation(std::string sdfs_filename);

    int store_operation();
    int get_sharded(std::string local_filename, std::string sdfs_filename_prefix);
private:
    std::string put_operation_master(tcp_client *client, std::string local_filename, std::string sdfs_filename);
    std::string get_operation_master(tcp_client *client, std::string local_filename, std::string sdfs_filename);
    std::string get_index_operation_master(tcp_client *client, std::string sdfs_filename);
    std::vector<std::string> append_operation_master(tcp_client *client, std::string metadata, std::string local_filename, std::string sdfs_filename);
    std::string get_metadata_operation_master(tcp_client *client, std::string sdfs_filename);
    int del_operation_master(tcp_client *client, std::string sdfs_filename);
    int ls_operation_master(tcp_client *client, std::string sdfs_filename);

    int get_operation_internal(tcp_client *client, std::string local_filename, std::string sdfs_filename);
    std::string get_metadata_operation_internal(tcp_client *client, std::string sdfs_filename);
    int put_operation_internal(tcp_client *client, std::string local_filename, std::string sdfs_filename);

    std::unique_ptr<tcp_client> get_master_socket();
    std::unique_ptr<tcp_client> get_internal_socket(std::string hostname);

    // master failure callback
    void send_files();

    // Services that we depend on
    election *el;
    std::unique_ptr<logger> lg;
    tcp_factory *fac;
    configuration *config;

    std::mt19937 mt;
    std::string mn_hostname;
};
