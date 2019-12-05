#pragma once

#include <string>

class maple_client {
public:
    virtual std::string get_error() = 0;

    virtual bool run_job(std::string maple_node, std::string maple_exe, int num_maples,
        std::string sdfs_intermediate_filename_prefix, std::string sdfs_src_dir) = 0;
};