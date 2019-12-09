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

    // Mutex for accessing files (very broad and very inefficient but it's just a mock)
    std::mutex file_mutex;

    // Services this depends on
    configuration *config;
};
