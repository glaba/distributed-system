#pragma once

#include "maple_client.h"
#include "environment.h"
#include "sdfs_client.h"
#include "configuration.h"
#include "tcp.h"
#include "logging.h"

#include <memory>

class maple_client_impl : public maple_client, public service_impl<maple_client_impl> {
public:
    maple_client_impl(environment &env);

    bool run_job(std::string maple_node, std::string maple_exe, int num_maples,
        std::string sdfs_intermediate_filename_prefix, std::string sdfs_src_dir);

private:
    sdfs_client *sdfsc;
    configuration *config;
    std::unique_ptr<tcp_client> client;
    std::unique_ptr<logger> lg;
};