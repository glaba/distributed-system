#pragma once

#include "sdfs.h"

#include <string>
#include <vector>
#include <functional>
#include <unordered_set>

class sdfs_master {
public:
    // Starts accepting and processing initial requests to master
    virtual void start() = 0;
    // Stops all master logic for the filesystem
    virtual void stop() = 0;
    // Register callbacks for various operations in the filesystem, which will be called if and only if
    // the file in question has metadata with keys matching one or more of the keys in the provided set
    virtual void on_put(const std::unordered_set<std::string> &keys, const sdfs::put_callback &callback) = 0;
    virtual void on_append(const std::unordered_set<std::string> &keys, const sdfs::append_callback &callback) = 0;
    virtual void on_get(const std::unordered_set<std::string> &keys, const sdfs::get_callback &callback) = 0;
    virtual void on_del(const std::unordered_set<std::string> &keys, const sdfs::del_callback &callback) = 0;
    // Returns a list of files / directories within the provided directory
    virtual std::optional<std::vector<std::string>> ls_files(std::string sdfs_dir) = 0;
    virtual std::optional<std::vector<std::string>> ls_dirs(std::string sdfs_dir) = 0;
    // Gets the metadata associated with a file in the SDFS
    virtual std::optional<sdfs_metadata> get_metadata(const std::string &sdfs_path) = 0;
    // Waits for all in-flight transactions to complete
    virtual void wait_transactions() = 0;
};
