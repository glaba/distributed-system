#pragma once

#include "sdfs_server.h"
#include "sdfs_message.h"
#include "sdfs_utils.hpp"
#include "election.h"
#include "logging.h"
#include "tcp.h"
#include "service.h"
#include "environment.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>


#include <cstring>

class sdfs_server_impl : public sdfs_server, public service_impl<sdfs_server_impl> {
public:
    sdfs_server_impl(environment &env);

    void start();
    void stop();

    int put_operation(int socket, std::string sdfs_filename);
    int get_operation(int socket, std::string sdfs_filename);
    int del_operation(int socket, std::string sdfs_filename);
    int ls_operation(int socket, std::string sdfs_filename);
    int get_metadata_operation(int socket, std::string sdfs_filename);
private:
    void process_loop();
    void handle_connection(int socket);

    int rep_operation(int socket, std::string sdfs_hostname, std::string sdfs_filename);
    int send_master_files(tcp_client *client);

    std::string get_files();
    int del_file(std::string sdfs_filename);
    bool file_exists(std::string sdfs_filename);

    // Services that we depend on
    election *el;
    std::unique_ptr<logger> lg;
    tcp_factory *fac;
    std::unique_ptr<tcp_server> server;
    configuration *config;
};
