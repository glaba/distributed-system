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

    std::string get_error();

    bool run_job(std::string juice_node, std::string local_exe, std::string juice_exe, int num_juices,
        partitioner::type partitioner_type, std::string sdfs_intermediate_filename_prefix, std::string sdfs_dest_filename);

private:
    std::string error = "";

    sdfs_client *sdfsc;
    configuration *config;
    tcp_factory *fac;
    std::unique_ptr<logger> lg;
};