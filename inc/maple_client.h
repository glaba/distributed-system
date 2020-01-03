#pragma once

#include <string>

class maple_client {
public:
    virtual auto get_error() const -> std::string = 0;

    virtual auto run_job(std::string const& maple_node, std::string const& local_exe, std::string const& maple_exe,
        int num_maples, std::string const& sdfs_src_dir, std::string const& sdfs_output_dir) -> bool = 0;
};