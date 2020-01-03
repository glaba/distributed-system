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
    auto put_operation(int socket, std::string const& sdfs_filename) -> int {return -1;}
    // handles a get request over the specified socket
    auto get_operation(int socket, std::string const& sdfs_filename) -> int {return -1;}
    // handles a del request over the specified socket
    auto del_operation(int socket, std::string const& sdfs_filename) -> int {return -1;}
    // handles a ls request over the specified socket
    auto ls_operation(int socket, std::string const& sdfs_filename) -> int {return -1;}
    // handles a get_metadata request over the specified socket
    auto get_metadata_operation(int socket, std::string const& sdfs_filename) -> int {return -1;}
};
