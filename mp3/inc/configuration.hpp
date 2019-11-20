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
    void set_hb_port(int port_) {
        hb_port = port_;
    }
    int get_hb_port() {
        return hb_port;
    }
    void set_hb_introducer(bool is_introducer_) {
        is_introducer = is_introducer_;
    }
    bool is_hb_introducer() {
        return is_introducer;
    }
    void set_election_port(int port_) {
        election_port = port_;
    }
    int get_election_port() {
        return election_port;
    }

private:
    std::string hostname;
    int hb_port;
    bool is_introducer;
    int election_port;

    configuration_impl() {}
};
