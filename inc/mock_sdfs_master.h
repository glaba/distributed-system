#pragma once

#include "sdfs_master.h"
#include "sdfs_client.h"
#include "mock_sdfs_client.h"
#include "environment.h"
#include "election.h"
#include "heartbeater.h"

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>

class mock_sdfs_master : public sdfs_master, public service_impl<mock_sdfs_master> {
public:
    mock_sdfs_master(environment &env);

    void start();
    void stop();

    void on_put(const std::unordered_set<std::string> &keys, const sdfs::put_callback &callback);
    void on_append(const std::unordered_set<std::string> &keys, const sdfs::append_callback &callback);
    void on_get(const std::unordered_set<std::string> &keys, const sdfs::get_callback &callback);
    void on_del(const std::unordered_set<std::string> &keys, const sdfs::del_callback &callback);
    std::optional<std::vector<std::string>> ls_files(std::string sdfs_dir);
    std::optional<std::vector<std::string>> ls_dirs(std::string sdfs_dir);
    std::optional<sdfs_metadata> get_metadata(const std::string &sdfs_path);
    void wait_transactions();

private:
    std::atomic<bool> running = false;

    mock_sdfs_client *client;
    election *el;
    heartbeater *hb;

    std::mutex callback_mutex;

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
