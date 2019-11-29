#pragma once

#include <string>

class configuration {
public:
    virtual void set_hostname(std::string hostname) = 0;
    virtual void set_first_node(bool first_node) = 0;
    virtual void set_hb_port(int port) = 0;
    virtual void set_election_port(int port) = 0;

    virtual std::string get_hostname() = 0;
    virtual bool is_first_node() = 0;
    virtual int get_hb_port() = 0;
    virtual int get_election_port() = 0;
};
