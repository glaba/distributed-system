#pragma once

#include "partitioner.h"

#include <string>

class juice_client {
public:
    virtual auto get_error() const -> std::string = 0;

    virtual auto run_job(std::string const& juice_node, std::string const& local_exe, std::string const& juice_exe, int num_juices,
        partitioner::type partitioner_type, std::string const& sdfs_src_dir, std::string const& sdfs_output_dir) -> bool = 0;
};