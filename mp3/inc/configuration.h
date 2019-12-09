#pragma once

#include <string>

class configuration {
public:
    virtual void set_hostname(std::string hostname) = 0;
    virtual void set_first_node(bool is_first_node_) = 0;
    virtual void set_hb_port(int port) = 0;
    virtual void set_election_port(int port) = 0;
    virtual void set_sdfs_internal_port(int port) = 0;
    virtual void set_sdfs_master_port(int port) = 0;
    virtual void set_maple_internal_port(int port) = 0;
    virtual void set_maple_master_port(int port) = 0;
    virtual void set_dir(std::string dir) = 0;
    virtual void set_sdfs_subdir(std::string subdir) = 0;
    virtual void set_maple_subdir(std::string subdir) = 0;

    virtual std::string get_hostname() = 0;
    virtual bool is_first_node() = 0;
    virtual int get_hb_port() = 0;
    virtual int get_election_port() = 0;
    virtual int get_sdfs_internal_port() = 0;
    virtual int get_sdfs_master_port() = 0;
    virtual int get_maple_internal_port() = 0;
    virtual int get_maple_master_port() = 0;
    virtual std::string get_dir() = 0;
    virtual std::string get_sdfs_dir() = 0;
    virtual std::string get_maple_dir() = 0;
};
