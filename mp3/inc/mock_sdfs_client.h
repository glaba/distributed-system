#pragma once

#include "sdfs_client.h"
#include "environment.h"
#include "configuration.h"

#include <atomic>
#include <string>
#include <functional>

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
    // handles a get request
    int get_operation(std::string local_filename, std::string sdfs_filename);
    // handles a del request
    int del_operation(std::string sdfs_filename);
    // handles an append request
    int append_operation(std::string local_filename, std::string sdfs_filename);
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
    class mock_sdfs_state : public service_state {
    public:
        std::string sdfs_dir;

        // A map from SDFS filename to the number of pieces there are for that file
        std::unordered_map<std::string, unsigned> num_pieces;
    };

    void access_pieces(std::function<void(std::unordered_map<std::string, unsigned>&)> callback);

    bool isolated = false;
    std::atomic<bool> running;

    std::string mn_hostname;

    // Mutex for accessing files (very broad and very inefficient but it's just a mock)
    std::mutex file_mutex;

    // Services this depends on
    configuration *config;
};
