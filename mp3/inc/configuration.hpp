#pragma once

#include "configuration.h"
#include "service.h"

class configuration_impl : public configuration, public service_impl<configuration_impl> {
public:
    configuration_impl(environment &env) {}

    void set_hostname(std::string hostname_) {
        hostname = hostname_;
    }
    std::string get_hostname() {
        return hostname;
    }
    void set_first_node(bool first_node_) {
        first_node = first_node_;
    }
    bool is_first_node() {
        return first_node;
    }
    void set_hb_port(int port_) {
        hb_port = port_;
    }
    int get_hb_port() {
        return hb_port;
    }
    void set_election_port(int port_) {
        election_port = port_;
    }
    int get_election_port() {
        return election_port;
    }
    void set_sdfs_port(int port_) {
        sdfs_port = port_;
    }
    int get_sdfs_port() {
        return sdfs_port;
    }
    void set_sdfs_dir(std::string dir) {
        sdfs_dir = dir;
    }
    std::string get_sdfs_dir() {
        return sdfs_dir;
    }

private:
    std::string hostname;
    bool first_node;
    int hb_port;
    int election_port;
    int sdfs_port;
    std::string sdfs_dir;

    configuration_impl() {}
};
