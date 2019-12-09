#pragma once

#include <string>
#include <vector>

class sdfs_master {
public:
    // Starts accepting and processing initial requests to master
    virtual void start() = 0;
    // Stops all master logic for the filesystem
    virtual void stop() = 0;
    // handles a put request over the specified socket
    virtual int put_operation(int socket, std::string sdfs_filename) = 0;
    // handles a get request over the specified socket
    virtual int get_operation(int socket, std::string sdfs_filename) = 0;
    // handles a del request over the specified socket
    virtual int del_operation(int socket, std::string sdfs_filename) = 0;
    // handles a ls request over the specified socket
    virtual int ls_operation(int socket, std::string sdfs_filename) = 0;
    // returns a list of sdfs files matching a given prefix
    virtual std::vector<std::string> get_files_by_prefix(std::string prefix) = 0;
};
