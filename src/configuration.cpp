#include "configuration.h"
#include "configuration.hpp"
#include "environment.h"

void configuration_impl::set_hostname(std::string const& hostname_) {
    hostname = hostname_;
}

auto configuration_impl::get_hostname() const -> std::string {
    return hostname;
}

void configuration_impl::set_first_node(bool is_first_node_) {
    first_node = is_first_node_;
}

auto configuration_impl::is_first_node() const -> bool {
    return first_node;
}

void configuration_impl::set_hb_port(int port_) {
    hb_port = port_;
}

auto configuration_impl::get_hb_port() const -> int {
    return hb_port;
}

void configuration_impl::set_election_port(int port_) {
    election_port = port_;
}

auto configuration_impl::get_election_port() const -> int {
    return election_port;
}

void configuration_impl::set_sdfs_internal_port(int port_) {
    sdfs_internal_port = port_;
}

auto configuration_impl::get_sdfs_internal_port() const -> int {
    return sdfs_internal_port;
}

void configuration_impl::set_sdfs_master_port(int port_) {
    sdfs_master_port = port_;
}

auto configuration_impl::get_sdfs_master_port() const -> int {
    return sdfs_master_port;
}

void configuration_impl::set_mj_internal_port(int port_) {
    mj_internal_port = port_;
}

auto configuration_impl::get_mj_internal_port() const -> int {
    return mj_internal_port;
}

void configuration_impl::set_mj_master_port(int port_) {
    mj_master_port = port_;
}

auto configuration_impl::get_mj_master_port() const -> int {
    return mj_master_port;
}

// Sets the directory that all files for the program will be stored in
// Assumes that the directory exist and is empty
void configuration_impl::set_dir(std::string const& dir_) {
    dir = dir_;
}

auto configuration_impl::get_dir() const -> std::string {
    return dir;
}

void configuration_impl::set_sdfs_subdir(std::string const& subdir) {
    sdfs_dir = dir + subdir + "/";
    // Create the directory
    if (mkdir(sdfs_dir.c_str(), ACCESSPERMS) != 0 && errno != EEXIST) {
        std::cerr << "Could not create SDFS subdirectory, exiting" << std::endl;
        exit(1);
    }
}

auto configuration_impl::get_sdfs_dir() const -> std::string {
    return sdfs_dir;
}

void configuration_impl::set_mj_subdir(std::string const& subdir) {
    mj_dir = dir + subdir + "/";
    if (mkdir(mj_dir.c_str(), ACCESSPERMS) != 0) {
        std::cout << "Directory is " << mj_dir << std::endl;
        std::cerr << "Could not create Maple subdirectory, exiting" << std::endl;
        exit(1);
    }
}

auto configuration_impl::get_mj_dir() const -> std::string {
    return mj_dir;
}

void configuration_test_impl::set_dir(std::string const& dir_) {
    // Create our own subdirectory within this directory only for files within this environment
    std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
    std::string subdir = "test" + std::to_string(mt());
    if (mkdir((dir_ + subdir).c_str(), ACCESSPERMS) != 0) {
        std::cerr << "Invalid directory provided, exiting" << std::endl;
        exit(1);
    }

    dir = dir_ + subdir + "/";
}

register_service<configuration, configuration_impl> register_configuration;
register_test_service<configuration, configuration_test_impl> register_test_configuration;
