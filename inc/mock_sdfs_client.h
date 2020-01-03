#pragma once

#include "sdfs_client.h"
#include "environment.h"
#include "configuration.h"
#include "logging.h"
#include "inputter.h"
#include "election.h"
#include "mt_safe.h"

#include <atomic>
#include <string>
#include <functional>
#include <set>

// Forward declaration so that it can be a friend of mock_sdfs_client
class mock_sdfs_master;

class mock_sdfs_client : public sdfs_client, public service_impl<mock_sdfs_client> {
public:
    mock_sdfs_client(environment &env);

    // Virtual methods for sdfs_client
    void start();
    void stop();
    void set_master_node(std::string const& hostname) {
        mn_hostname = hostname;
    }
    auto put(std::string const& local_filename, std::string const& sdfs_path, sdfs_metadata const& metadata) -> int;
    auto put(inputter<std::string> const& in, std::string const& sdfs_path, sdfs_metadata const& metadata) -> int;
    auto append(std::string const& local_filename, std::string const& sdfs_path, sdfs_metadata const& metadata) -> int;
    auto append(inputter<std::string> const& in, std::string const& sdfs_path, sdfs_metadata const& metadata) -> int;
    auto get(std::string const& local_filename, std::string const& sdfs_path) -> int;
    auto get_metadata(std::string const& sdfs_path) -> std::optional<sdfs_metadata>;
    auto del(std::string const& sdfs_path) -> int;
    auto mkdir(std::string const& sdfs_dir) -> int;
    auto rmdir(std::string const& sdfs_dir) -> int;
    auto ls_files(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>>;
    auto ls_dirs(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>>;
    auto get_num_shards(std::string const& sdfs_path) -> int;

    // Virtual method for service_impl
    auto init_state() -> std::unique_ptr<service_state>;

    auto get_sdfs_root_dir() -> std::string;

private:
    enum op_type {
        op_put, op_append, op_get, op_del
    };

    struct dir_state {
        // A set of files and directories within the directory, name only
        std::unordered_set<std::string> files;
        std::unordered_set<std::string> dirs;
        bool being_deleted;
    };

    struct file_state {
        bool being_deleted;

        // Mutex protecting access to the physical filesystem for this file as well as the rest of this data structure
        std::unique_ptr<std::recursive_mutex> file_mutex;
        unsigned num_pieces;
        sdfs_metadata metadata;
    };

    using dir_state_map = std::unordered_map<sdfs::internal_path, dir_state, sdfs::internal_path_hash>;
    using file_state_map = std::unordered_map<sdfs::internal_path, file_state, sdfs::internal_path_hash>;
    using master_callback_type = std::function<void(op_type, file_state_map::iterator)>;

    class mock_sdfs_state : public service_state {
    public:
        std::string sdfs_root_dir;

        // A map from directories to the state of that directory
        dir_state_map dir_states;

        // A map from SDFS path of a file to the state of a file at that path
        file_state_map file_states;

        // A callback to the master node when an operation occurs in the SDFS
        master_callback_type master_callback;
    };

    // Helper function used to safely update the mock_sdfs_state and subsequently access its members
    // The callback function is provided with an iterator pointing to the entry for the file in file_states
    auto access_helper(std::string const& sdfs_path, std::function<bool(file_state_map::iterator, master_callback_type const&)> const& callback, op_type type) -> int;
    // Backing functions for the two versions of put and append
    auto write(std::string const& local_filename, std::string const& sdfs_path, sdfs_metadata const& metadata, bool is_append) -> int;
    auto write(inputter<std::string> const& in, std::string const& sdfs_path, sdfs_metadata const& metadata, bool is_append) -> int;

    void get_children(sdfs::internal_path const& dir, dir_state_map &dir_states,
        std::vector<sdfs::internal_path> &subdirs, std::vector<sdfs::internal_path> &subfiles);
    void access_pieces(std::function<void(dir_state_map&, file_state_map&, master_callback_type const&)> callback);

    std::string mn_hostname;

    // Services this depends on
    configuration *config;
    std::unique_ptr<logger> lg;
    election *el;

    // RNG to generate temporary filenames
    mt_safe mt;

    // mock_sdfs_master essentially relies completely on mock_sdfs_client for all functionality
    friend class mock_sdfs_master;

    // Methods and data to facilitate mock_sdfs_master
    // Inserts a transaction into transaction_timestamps and returns the timestamp of the transaction
    auto mark_transaction_started() -> uint64_t;
    // Removes a transaction from transaction_timestamps
    void mark_transaction_completed(uint64_t timestamp);
    // Gets the time of the earliest transaction
    auto get_earliest_transaction() const -> uint32_t;
    // Waits for all transactions starting before the time of calling to complete
    void wait_transactions() const;
    // Set acting as a priority queue of transaction initiation timestamps
    // Upper 32 bits are timestamp, lower 32 bits are equal to tx_counter for uniqueness
    mutable std::mutex tx_times_mutex;
    uint32_t tx_counter = 0;
    std::set<uint64_t> transaction_timestamps;

    void on_event(master_callback_type const& callback);
};
