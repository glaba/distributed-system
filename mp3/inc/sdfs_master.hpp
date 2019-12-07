#pragma once

#include "sdfs_master.h"
#include "sdfs_message.h"
#include "sdfs_utils.hpp"
#include "member_list.h"
#include "heartbeater.h"
#include "election.h"
#include "logging.h"
#include "tcp.h"
#include "service.h"
#include "environment.h"

#include <string>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#define NUM_REPLICAS 4

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

    // used to replicate a given file
    int rep_operation(int socket, std::string hostname, std::string sdfs_filename);
    // used to receive a list of files over a socket
    int files_operation(int socket, std::string hostname, std::string data);

    bool sdfs_file_exists(std::string sdfs_filename);
    std::vector<std::string> get_hostnames();

    // Services that we depend on
    election *el;
    heartbeater *hb;
    std::unique_ptr<logger> lg;
    std::unique_ptr<tcp_client> client;
    std::unique_ptr<tcp_server> server;
    std::unique_ptr<std::unordered_map<std::string, std::vector<std::string>>> file_to_hostnames;
    std::unique_ptr<std::unordered_map<std::string, std::vector<std::string>>> hostname_to_files;
    configuration *config;
};
