#pragma once

#include <string>

class sdfs_server {
public:
    // Starts accepting and processing client requests
    virtual void start() = 0;
    // Stops all server logic for the filesystem
    virtual void stop() = 0;
    // handles a put request over the specified socket
    virtual auto put_operation(int socket, std::string const& sdfs_filename) -> int = 0;
    // handles a get request over the specified socket
    virtual auto get_operation(int socket, std::string const& sdfs_filename) -> int = 0;
    // handles a del request over the specified socket
    virtual auto del_operation(int socket, std::string const& sdfs_filename) -> int = 0;
    // handles a ls request over the specified socket
    virtual auto ls_operation(int socket, std::string const& sdfs_filename) -> int = 0;
    // handles a get_metadata request over the specified socket
    virtual auto get_metadata_operation(int socket, std::string const& sdfs_filename) -> int = 0;
};
