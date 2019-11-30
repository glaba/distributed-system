#pragma once

#include <string>

class sdfs_server {
public:
    // Starts accepting and processing client requests
    virtual void start() = 0;
    // Stops all server logic for the filesystem
    virtual void stop() = 0;
    /*
    // handles a put request over the specified socket
    virtual int put_operation(int socket, std::string sdfs_filename) = 0;
    // handles a get request over the specified socket
    virtual int get_operation(int socket, std::string sdfs_filename) = 0;
    // handles a del request over the specified socket
    virtual int del_operation(int socket, std::string sdfs_filename) = 0;
    // handles a ls request over the specified socket
    virtual int ls_operation(int socket, std::string sdfs_filename) = 0;
    */
};
