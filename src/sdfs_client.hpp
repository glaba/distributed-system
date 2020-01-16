#pragma once

#include "sdfs_client.h"
#include "sdfs_message.h"
#include "sdfs_utils.hpp"
#include "election.h"
#include "logging.h"
#include "tcp.h"
#include "service.h"
#include "environment.h"

#include <sys/types.h>
#include <dirent.h>

#include <string.h>
#include <random>
#include <stdlib.h>
#include <memory>

// Defining the return value for failed operations
#define SDFS_CLIENT_FAILURE -1
// Defining the return value for successful operations
#define SDFS_CLIENT_SUCCESS 0

class sdfs_client_impl : public sdfs_client, public service_impl<sdfs_client_impl> {
public:
    sdfs_client_impl(environment &env);

    void start();
    void stop();
    void set_master_node(std::string const& hostname) {
        mn_hostname = hostname;
    }
    auto put(std::string const& local_filename, std::string const& sdfs_path, sdfs_metadata const& metadata = sdfs_metadata()) -> int;
    auto put(inputter<std::string> const& in, std::string const& sdfs_path, sdfs_metadata const& metadata = sdfs_metadata()) -> int;
    auto append(std::string const& local_filename, std::string const& sdfs_path, sdfs_metadata const& metadata = sdfs_metadata()) -> int;
    auto append(inputter<std::string> const& in, std::string const& sdfs_path, sdfs_metadata const& metadata = sdfs_metadata()) -> int;
    auto get(std::string const& local_filename, std::string const& sdfs_path) -> int;
    auto get_metadata(std::string const& sdfs_path) -> std::optional<sdfs_metadata>;
    auto del(std::string const& sdfs_path) -> int;
    auto mkdir(std::string const& sdfs_dir) -> int;
    auto rmdir(std::string const& sdfs_dir) -> int;
    auto ls_files(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>>;
    auto ls_dirs(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>>;

private:
    // Services that we depend on
    election *el;
    std::unique_ptr<logger> lg;
    tcp_factory *fac;
    configuration *config;

    std::mt19937 mt;
    std::string mn_hostname;
};
