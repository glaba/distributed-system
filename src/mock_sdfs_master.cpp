#include "mock_sdfs_master.h"

#include <stdlib.h>
#include <unordered_set>
#include <iostream>

using std::string;
using std::unique_ptr;
using namespace sdfs;

mock_sdfs_master::mock_sdfs_master(environment &env)
    : client(dynamic_cast<mock_sdfs_client*>(env.get<sdfs_client>()))
    , el(env.get<election>())
    , hb(env.get<heartbeater>()) {}

void mock_sdfs_master::start() {
    if (running.load()) {
        return;
    }
    running = true;

    std::thread election_thread([&] {
        bool is_master;
        do {
            el->wait_master_node([&] (member m) {
                is_master = (m.id == hb->get_id());
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        } while (!is_master && running.load());
        if (!running.load()) {
            return;
        }

        // Register special master callback that only mock_sdfs_master has access to
        client->on_event([this] (mock_sdfs_client::op_type type, mock_sdfs_client::file_state_map::iterator it) {
            unsigned index = it->second.num_pieces - 1;
            sdfs_metadata metadata = it->second.metadata;
            string sdfs_path = sdfs::deconvert_path(it->first);

            std::lock_guard<std::mutex> guard(callback_mutex);
            for (auto &[key, _] : metadata) {
                switch (type) {
                    case mock_sdfs_client::op_type::op_put:
                        for (sdfs::put_callback *cb : put_callbacks[key]) {
                            (*cb)(sdfs_path, metadata);
                        }
                        break;
                    case mock_sdfs_client::op_type::op_append:
                        for (sdfs::append_callback *cb : append_callbacks[key]) {
                            (*cb)(sdfs_path, index, metadata);
                        }
                        break;
                    case mock_sdfs_client::op_type::op_get:
                        for (sdfs::get_callback *cb : get_callbacks[key]) {
                            (*cb)(sdfs_path, metadata);
                        }
                        break;
                    case mock_sdfs_client::op_type::op_del:
                        for (sdfs::del_callback *cb : del_callbacks[key]) {
                            (*cb)(sdfs_path, metadata);
                        }
                        break;
                    default: assert(false);
                }
            }
        });
    });
    election_thread.detach();
}

void mock_sdfs_master::stop() {
    running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void mock_sdfs_master::on_put(const std::unordered_set<string> &keys, const sdfs::put_callback &callback) {
    std::lock_guard<std::mutex> guard(callback_mutex);
    sdfs::put_callback *cb_ptr = new put_callback(callback);
    put_callback_pool.push_back(unique_ptr<sdfs::put_callback>(cb_ptr));
    for (auto &key : keys) {
        put_callbacks[key].push_back(cb_ptr);
    }
}

void mock_sdfs_master::on_append(const std::unordered_set<string> &keys, const sdfs::append_callback &callback) {
    std::lock_guard<std::mutex> guard(callback_mutex);
    sdfs::append_callback *cb_ptr = new append_callback(callback);
    append_callback_pool.push_back(unique_ptr<sdfs::append_callback>(cb_ptr));
    for (auto &key : keys) {
        append_callbacks[key].push_back(cb_ptr);
    }
}

void mock_sdfs_master::on_get(const std::unordered_set<string> &keys, const sdfs::get_callback &callback) {
    std::lock_guard<std::mutex> guard(callback_mutex);
    sdfs::get_callback *cb_ptr = new get_callback(callback);
    get_callback_pool.push_back(unique_ptr<sdfs::get_callback>(cb_ptr));
    for (auto &key : keys) {
        get_callbacks[key].push_back(cb_ptr);
    }
}

void mock_sdfs_master::on_del(const std::unordered_set<string> &keys, const sdfs::del_callback &callback) {
    std::lock_guard<std::mutex> guard(callback_mutex);
    sdfs::del_callback *cb_ptr = new del_callback(callback);
    del_callback_pool.push_back(unique_ptr<sdfs::del_callback>(cb_ptr));
    for (auto &key : keys) {
        del_callbacks[key].push_back(cb_ptr);
    }
}

std::optional<std::vector<string>> mock_sdfs_master::ls_files(string sdfs_dir) {
    return client->ls_files(sdfs_dir);
}

std::optional<std::vector<string>> mock_sdfs_master::ls_dirs(string sdfs_dir) {
    return client->ls_dirs(sdfs_dir);
}

std::optional<sdfs_metadata> mock_sdfs_master::get_metadata(const string &sdfs_path) {
    return client->get_metadata(sdfs_path);
}

void mock_sdfs_master::wait_transactions() {
    client->wait_transactions();
}

register_test_service<sdfs_master, mock_sdfs_master> register_mock_sdfs_master;
