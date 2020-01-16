#pragma once

#include "sdfs.h"
#include "inputter.h"

#include <string>
#include <optional>
#include <vector>
#include <unordered_map>

class sdfs_client {
public:
    // Starts accepting and processing CLI inputs
    virtual void start() = 0;
    // Stops all client logic for the filesystem
    virtual void stop() = 0;
    // Optionally sets the master node, if this sdfs_client is not running in an environment with election
    virtual void set_master_node(std::string const& hostname) = 0;
    // Puts some data into a file in the SDFS, overwriting whatever was previously there
    virtual auto put(std::string const& local_filename, std::string const& sdfs_path, sdfs_metadata const& metadata = sdfs_metadata()) -> int = 0;
    virtual auto put(inputter<std::string> const& in, std::string const& sdfs_path, sdfs_metadata const& metadata = sdfs_metadata()) -> int = 0;
    // Appends the provided data to a file in the SDFS
    virtual auto append(std::string const& local_filename, std::string const& sdfs_path, sdfs_metadata const& metadata = sdfs_metadata()) -> int = 0;
    virtual auto append(inputter<std::string> const& in, std::string const& sdfs_path, sdfs_metadata const& metadata = sdfs_metadata()) -> int = 0;
    // Gets a file from the SDFS, storing it into a local file
    virtual auto get(std::string const& local_filename, std::string const& sdfs_path) -> int = 0;
    // Gets the metadata associated with a file in the SDFS
    virtual auto get_metadata(std::string const& sdfs_path) -> std::optional<sdfs_metadata> = 0;
    // Deletes a file in the SDFS
    virtual auto del(std::string const& sdfs_path) -> int = 0;
    // Creates a subdirectory in the SDFS
    virtual auto mkdir(std::string const& sdfs_dir) -> int = 0;
    // Removes a subdirectory from the SDFS, deleting all the files within
    virtual auto rmdir(std::string const& sdfs_dir) -> int = 0;
    // Returns a list of files / directories within the provided directory
    virtual auto ls_files(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>> = 0;
    virtual auto ls_dirs(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>> = 0;
};
