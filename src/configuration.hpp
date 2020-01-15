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

    void set_hostname(std::string const& hostname);
    void set_first_node(bool is_first_node_);
    void set_hb_port(int port);
    void set_election_port(int port);
    void set_sdfs_internal_port(int port);
    void set_sdfs_master_port(int port);
    void set_mj_internal_port(int port);
    void set_mj_master_port(int port);
    void set_dir(std::string const& dir);
    void set_sdfs_subdir(std::string const& subdir);
    void set_mj_subdir(std::string const& subdir);
    void set_shared_config_subdir(std::string const& subdir);

    auto get_hostname() const -> std::string;
    auto is_first_node() const -> bool;
    auto get_hb_port() const -> int;
    auto get_election_port() const -> int;
    auto get_sdfs_internal_port() const -> int;
    auto get_sdfs_master_port() const -> int;
    auto get_mj_internal_port() const -> int;
    auto get_mj_master_port() const -> int;
    auto get_dir() const -> std::string;
    auto get_sdfs_dir() const -> std::string;
    auto get_mj_dir() const -> std::string;
    auto get_shared_config_dir() const -> std::string;

protected:
    std::string hostname;
    bool first_node;
    int hb_port;
    int election_port;
    int sdfs_internal_port;
    int sdfs_master_port;
    int mj_internal_port;
    int mj_master_port;
    std::string dir;
    std::string sdfs_dir;
    std::string mj_dir;
    std::string shared_config_dir;

    configuration_impl() {}
};

class configuration_test_impl : public configuration_impl {
public:
    configuration_test_impl(environment &env) {}

    void set_dir(std::string const& dir_);
};
