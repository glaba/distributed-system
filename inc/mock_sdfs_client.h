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
    void set_master_node(const std::string &hostname) {
        mn_hostname = hostname;
    }
    int put(const std::string &local_filename, const std::string &sdfs_path, const sdfs_metadata &metadata);
    int put(const inputter<std::string> &in, const std::string &sdfs_path, const sdfs_metadata &metadata);
    int append(const std::string &local_filename, const std::string &sdfs_path, const sdfs_metadata &metadata);
    int append(const inputter<std::string> &in, const std::string &sdfs_path, const sdfs_metadata &metadata);
    int get(const std::string &local_filename, const std::string &sdfs_path);
    std::optional<sdfs_metadata> get_metadata(const std::string &sdfs_path);
    int del(const std::string &sdfs_path);
    int mkdir(const std::string &sdfs_dir);
    int rmdir(const std::string &sdfs_dir);
    std::optional<std::vector<std::string>> ls_files(const std::string &sdfs_dir);
    std::optional<std::vector<std::string>> ls_dirs(const std::string &sdfs_dir);
    int get_num_shards(const std::string &sdfs_path);

    // Virtual method for service_impl
    std::unique_ptr<service_state> init_state();

    std::string get_sdfs_root_dir();

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
    int access_helper(const std::string &sdfs_path, std::function<bool(file_state_map::iterator, master_callback_type const&)> callback, op_type type);
    // Backing functions for the two versions of put and append
    int write(const std::string &local_filename, const std::string &sdfs_path, const sdfs_metadata &metadata, bool is_append);
    int write(const inputter<std::string> &in, const std::string &sdfs_path, const sdfs_metadata &metadata, bool is_append);

    void get_children(sdfs::internal_path dir, dir_state_map &dir_states,
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
    uint64_t mark_transaction_started();
    // Removes a transaction from transaction_timestamps
    void mark_transaction_completed(uint64_t timestamp);
    // Gets the time of the earliest transaction
    uint32_t get_earliest_transaction();
    // Waits for all transactions starting before the time of calling to complete
    void wait_transactions();
    // Set acting as a priority queue of transaction initiation timestamps
    // Upper 32 bits are timestamp, lower 32 bits are equal to tx_counter for uniqueness
    std::mutex tx_times_mutex;
    uint32_t tx_counter = 0;
    std::set<uint64_t> transaction_timestamps;

    void on_event(const master_callback_type &callback);
};
