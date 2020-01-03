#pragma once

#include "juice_client.h"
#include "environment.h"
#include "sdfs_client.h"
#include "configuration.h"
#include "tcp.h"
#include "logging.h"

#include <memory>

class juice_client_impl : public juice_client, public service_impl<juice_client_impl> {
public:
    juice_client_impl(environment &env);

    auto get_error() const -> std::string;

    auto run_job(std::string const& juice_node, std::string const& local_exe, std::string const& juice_exe, int num_juices,
        partitioner::type partitioner_type, std::string const& sdfs_src_dir, std::string const& sdfs_output_dir) -> bool;

private:
    std::string error = "";

    sdfs_client *sdfsc;
    configuration *config;
    tcp_factory *fac;
    std::unique_ptr<logger> lg;
};