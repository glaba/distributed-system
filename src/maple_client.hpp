#pragma once

#include "maple_client.h"
#include "environment.h"
#include "sdfs_client.h"
#include "configuration.h"
#include "tcp.h"
#include "logging.h"

#include <memory>
#include <string>
#include <optional>

class maple_client_impl : public maple_client, public service_impl<maple_client_impl> {
public:
    maple_client_impl(environment &env);

    auto get_error() const -> std::string;

    template <typename Msg>
    auto filter_msg(std::string const& msg_str) -> std::optional<Msg>;

    auto run_job(std::string const& maple_node, std::string const& local_exe, std::string const& maple_exe,
        int num_maples, std::string const& sdfs_intermediate_filename_prefix, std::string const& sdfs_src_dir) -> bool;

private:
    std::string error = "";

    sdfs_client *sdfsc;
    configuration *config;
    tcp_factory *fac;
    std::unique_ptr<logger> lg;
};