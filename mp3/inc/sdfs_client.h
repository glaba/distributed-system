#pragma once

#include <string>

class sdfs_client {
public:
    // Starts accepting and processing CLI inputs
    virtual void start() = 0;
    // Stops all client logic for the filesystem
    virtual void stop() = 0;
    // Optionally sets the master node, if this sdfs_client is not running in an environment with election
    virtual void set_master_node(std::string hostname) = 0;
    // handles a put request over the specified socket
    virtual int put_operation(std::string local_filename, std::string sdfs_filename) = 0;
    // handles a get request over the specified socket
    virtual int get_operation(std::string local_filename, std::string sdfs_filename) = 0;
    // handles a del request over the specified socket
    virtual int del_operation(std::string sdfs_filename) = 0;
    // handles a ls request over the specified socket
    virtual int ls_operation(std::string sdfs_filename) = 0;
    // handles a append request over the specified socket
    virtual int append_operation(std::string local_filename, std::string sdfs_filename) = 0;
    // handles a store request
    virtual int store_operation() = 0;
    // handles a get_metadata request over the specified socket
    virtual std::string get_metadata_operation(std::string sdfs_filename) = 0;

    virtual int get_sharded(std::string local_filename, std::string sdfs_filename_prefix) = 0;
};
