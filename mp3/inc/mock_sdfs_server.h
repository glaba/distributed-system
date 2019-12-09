#pragma once

#include "sdfs_server.h"
#include "environment.h"

class mock_sdfs_server : public sdfs_server, public service_impl<mock_sdfs_server> {
public:
    mock_sdfs_server(environment &env) {}
    // Starts accepting and processing client requests
    void start() {};
    // Stops all server logic for the filesystem
    void stop() {};
    // handles a put request over the specified socket
    int put_operation(int socket, std::string sdfs_filename) {return -1;}
    // handles a get request over the specified socket
    int get_operation(int socket, std::string sdfs_filename) {return -1;}
    // handles a del request over the specified socket
    int del_operation(int socket, std::string sdfs_filename) {return -1;}
    // handles a ls request over the specified socket
    int ls_operation(int socket, std::string sdfs_filename) {return -1;}
    // handles a get_metadata request over the specified socket
    int get_metadata_operation(int socket, std::string sdfs_filename) {return -1;}
};
