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
    virtual void set_master_node(const std::string &hostname) = 0;
    // Puts some data into a file in the SDFS, overwriting whatever was previously there
    virtual int put(const std::string &local_filename, const std::string &sdfs_path, const sdfs_metadata &metadata = sdfs_metadata()) = 0;
    virtual int put(const inputter<std::string> &in, const std::string &sdfs_path, const sdfs_metadata &metadata = sdfs_metadata()) = 0;
    // Appends the provided data to a file in the SDFS
    virtual int append(const std::string &local_filename, const std::string &sdfs_path, const sdfs_metadata &metadata = sdfs_metadata()) = 0;
    virtual int append(const inputter<std::string> &in, const std::string &sdfs_path, const sdfs_metadata &metadata = sdfs_metadata()) = 0;
    // Gets a file from the SDFS, storing it into a local file
    virtual int get(const std::string &local_filename, const std::string &sdfs_path) = 0;
    // Gets the metadata associated with a file in the SDFS
    virtual std::optional<sdfs_metadata> get_metadata(const std::string &sdfs_path) = 0;
    // Deletes a file in the SDFS
    virtual int del(const std::string &sdfs_path) = 0;
    // Creates a subdirectory in the SDFS
    virtual int mkdir(const std::string &sdfs_dir) = 0;
    // Removes a subdirectory from the SDFS, deleting all the files within
    virtual int rmdir(const std::string &sdfs_dir) = 0;
    // Returns a list of files / directories within the provided directory
    virtual std::optional<std::vector<std::string>> ls_files(const std::string &sdfs_dir) = 0;
    virtual std::optional<std::vector<std::string>> ls_dirs(const std::string &sdfs_dir) = 0;
    // Gets the number of shards that a file is split into
    virtual int get_num_shards(const std::string &sdfs_path) = 0;
};
