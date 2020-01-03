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
    virtual void on_put(std::unordered_set<std::string> const& keys, sdfs::put_callback const& callback) = 0;
    virtual void on_append(std::unordered_set<std::string> const& keys, sdfs::append_callback const& callback) = 0;
    virtual void on_get(std::unordered_set<std::string> const& keys, sdfs::get_callback const& callback) = 0;
    virtual void on_del(std::unordered_set<std::string> const& keys, sdfs::del_callback const& callback) = 0;
    // Returns a list of files / directories within the provided directory
    virtual auto ls_files(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>> = 0;
    virtual auto ls_dirs(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>> = 0;
    // Gets the metadata associated with a file in the SDFS
    virtual auto get_metadata(std::string const& sdfs_path) -> std::optional<sdfs_metadata> = 0;
    // Waits for all in-flight transactions to complete
    virtual void wait_transactions() const = 0;
};
