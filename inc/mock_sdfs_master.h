#pragma once

#include "sdfs_master.h"
#include "sdfs_client.h"
#include "mock_sdfs_client.h"
#include "environment.h"
#include "election.h"
#include "heartbeater.h"
#include "locking.h"

#include <vector>
#include <string>
#include <memory>
#include <atomic>

class mock_sdfs_master : public sdfs_master, public service_impl<mock_sdfs_master> {
public:
    mock_sdfs_master(environment &env);

    void start();
    void stop();

    void on_put(std::unordered_set<std::string> const& keys, sdfs::put_callback const& callback);
    void on_append(std::unordered_set<std::string> const& keys, sdfs::append_callback const& callback);
    void on_get(std::unordered_set<std::string> const& keys, sdfs::get_callback const& callback);
    void on_del(std::unordered_set<std::string> const& keys, sdfs::del_callback const& callback);
    auto ls_files(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>>;
    auto ls_dirs(std::string const& sdfs_dir) -> std::optional<std::vector<std::string>>;
    auto get_metadata(std::string const& sdfs_path) -> std::optional<sdfs_metadata>;
    void wait_transactions() const;

private:
    std::atomic<bool> running = false;

    mock_sdfs_client *client;
    election *el;
    heartbeater *hb;

    struct callback_state {
        // A map from metadata key to the callbacks to be called for that key
        std::unordered_map<std::string, std::vector<sdfs::put_callback*>> put_callbacks;
        std::unordered_map<std::string, std::vector<sdfs::append_callback*>> append_callbacks;
        std::unordered_map<std::string, std::vector<sdfs::get_callback*>> get_callbacks;
        std::unordered_map<std::string, std::vector<sdfs::del_callback*>> del_callbacks;

        // A map from callback ID to the actual callback function
        std::vector<std::unique_ptr<sdfs::put_callback>> put_callback_pool;
        std::vector<std::unique_ptr<sdfs::append_callback>> append_callback_pool;
        std::vector<std::unique_ptr<sdfs::get_callback>> get_callback_pool;
        std::vector<std::unique_ptr<sdfs::del_callback>> del_callback_pool;
    };
    locked<callback_state> callbacks_lock;
};
