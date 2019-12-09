#pragma once

#include "configuration.h"
#include "service.h"

#include <errno.h>
#include <sys/stat.h>
#include <random>
#include <chrono>
#include <string>
#include <iostream>

extern int errno;

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
    void set_sdfs_internal_port(int port_) {
        sdfs_internal_port = port_;
    }
    int get_sdfs_internal_port() {
        return sdfs_internal_port;
    }
    void set_sdfs_master_port(int port_) {
        sdfs_master_port = port_;
    }
    int get_sdfs_master_port() {
        return sdfs_master_port;
    }
    // Sets the directory that all files for the program will be stored in
    // Assumes that the directory exist and is empty
    void set_dir(std::string dir_) {
        dir = dir_;
    }
    std::string get_dir() {
        return dir;
    }
    void set_sdfs_subdir(std::string subdir) {
        sdfs_dir = dir + subdir + "/";
        // Create the directory
        if (mkdir(sdfs_dir.c_str(), ACCESSPERMS) != 0 && errno != EEXIST) {
            std::cerr << "Could not create SDFS subdirectory, exiting" << std::endl;
            exit(1);
        }
    }
    std::string get_sdfs_dir() {
        return sdfs_dir;
    }

protected:
    std::string hostname;
    bool first_node;
    int hb_port;
    int election_port;
    int sdfs_internal_port;
    int sdfs_master_port;
    std::string dir;
    std::string sdfs_dir;

    configuration_impl() {}
};

class configuration_test_impl : public configuration_impl {
public:
    configuration_test_impl(environment &env) {}

    void set_dir(std::string dir_) {
        // Create our own subdirectory within this directory only for files within this environment
        std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
        std::string subdir = "test" + std::to_string(mt());
        if (mkdir((dir_ + subdir).c_str(), ACCESSPERMS) != 0) {
            std::cerr << "Invalid directory provided, exiting" << std::endl;
            exit(1);
        }

        dir = dir_ + subdir + "/";
    }
};
