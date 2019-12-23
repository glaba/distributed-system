#pragma once

#include "sdfs_client.h"
#include "environment.h"
#include "configuration.h"
#include "inputter.h"

#include <atomic>
#include <string>
#include <functional>
#include <random>

class mock_sdfs_client : public sdfs_client, public service_impl<mock_sdfs_client> {
public:
    mock_sdfs_client(environment &env);

    // Starts accepting and processing CLI inputs
    void start();
    // Stops all client logic for the filesystem
    void stop();
    // Optionally sets the master node, if this sdfs_client is not running in an environment with election
    void set_master_node(std::string hostname) {
        mn_hostname = hostname; // TODO: implement this functionality
    }
    // handles a put request
    int put_operation(std::string local_filename, std::string sdfs_filename);
    int put_operation(inputter<std::string> in, std::string sdfs_filename);
    // handles a get request
    int get_operation(std::string local_filename, std::string sdfs_filename);
    // handles a del request
    int del_operation(std::string sdfs_filename);
    // handles an append request
    int append_operation(std::string local_filename, std::string sdfs_filename);
    int append_operation(inputter<std::string> in, std::string sdfs_filename);
    // handles a ls request
    int ls_operation(std::string sdfs_filename);
    // handles a store request
    int store_operation();
    // handles a get_metadata request
    // TODO: implement this
    std::string get_metadata_operation(std::string sdfs_filename) {return "";}
    int get_sharded(std::string local_filename, std::string sdfs_filename_prefix);
    int get_index_operation(std::string sdfs_filename);

    std::unique_ptr<service_state> init_state();

    std::string get_sdfs_dir();

    void isolate() {
        isolated = true;
    }

private:
    int put_helper(std::string sdfs_filename, std::function<bool()> callback);
    int append_helper(std::string sdfs_filename, std::function<bool(unsigned)> callback);

    class mock_sdfs_state : public service_state {
    public:
        std::string sdfs_dir;

        // A map from SDFS filename to the number of pieces there are for that file, and a mutex for each
        std::unordered_map<std::string, std::unique_ptr<std::mutex>> file_mutexes;
        // If num_pieces is 0, that means that any operation done on the file must result in failure
        // This prevents deletion of a file from racing with puts and appends
        std::unordered_map<std::string, unsigned> num_pieces;
    };

    void access_pieces(std::function<void(std::unordered_map<std::string, unsigned>&,
        std::unordered_map<std::string, std::unique_ptr<std::mutex>>&)> callback);

    bool isolated = false;
    std::atomic<bool> running;

    std::string mn_hostname;

    // Services this depends on
    configuration *config;

    // RNG to generate temporary filenames
    std::mt19937 mt;
};
