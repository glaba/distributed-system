#pragma once

#include "member_list.h"

#include <string>

class sdfs_client {
public:
    // Starts accepting and processing CLI inputs
    virtual void start() = 0;
    // Stops all client logic for the filesystem
    virtual void stop() = 0;
    /*
    // handles a put request over the specified socket
    virtual int put_operation(int socket, std::string local_filename, std::string sdfs_filename) = 0;
    // handles a get request over the specified socket
    virtual int get_operation(int socket, std::string local_filename, std::string sdfs_filename) = 0;
    // handles a del request over the specified socket
    virtual int del_operation(int socket, std::string sdfs_filename) = 0;
    // handles a ls request over the specified socket
    virtual int ls_operation(int socket, std::string sdfs_filename) = 0;
    // handles a store request
    virtual int store_operation() = 0;
    */
};
