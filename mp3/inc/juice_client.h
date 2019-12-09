#pragma once

#include "partitioner.h"

#include <string>

class juice_client {
public:
    virtual std::string get_error() = 0;

    virtual bool run_job(std::string juice_node, std::string local_exe, std::string juice_exe, int num_juices,
        partitioner::type partitioner_type, std::string sdfs_intermediate_filename_prefix, std::string sdfs_dest_filename) = 0;
};