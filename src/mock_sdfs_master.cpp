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
        client->on_event([this]
            (mock_sdfs_client::op_type type, string const& sdfs_path, unlocked<mock_sdfs_client::file_state> &f_state)
        {
            unsigned index = f_state->num_pieces - 1;
            sdfs_metadata metadata = f_state->metadata;

            unlocked<callback_state> callbacks = callbacks_lock();
            for (auto const& [key, _] : metadata) {
                switch (type) {
                    case mock_sdfs_client::op_type::op_put:
                        for (sdfs::put_callback *cb : callbacks->put_callbacks[key]) {
                            (*cb)(sdfs_path, metadata);
                        }
                        break;
                    case mock_sdfs_client::op_type::op_append:
                        for (sdfs::append_callback *cb : callbacks->append_callbacks[key]) {
                            (*cb)(sdfs_path, index, metadata);
                        }
                        break;
                    case mock_sdfs_client::op_type::op_get:
                        for (sdfs::get_callback *cb : callbacks->get_callbacks[key]) {
                            (*cb)(sdfs_path, metadata);
                        }
                        break;
                    case mock_sdfs_client::op_type::op_del:
                        for (sdfs::del_callback *cb : callbacks->del_callbacks[key]) {
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

void mock_sdfs_master::on_put(std::unordered_set<string> const& keys, sdfs::put_callback const& callback) {
    unlocked<callback_state> callbacks = callbacks_lock();
    sdfs::put_callback *cb_ptr = new put_callback(callback);
    callbacks->put_callback_pool.push_back(unique_ptr<sdfs::put_callback>(cb_ptr));
    for (auto const& key : keys) {
        callbacks->put_callbacks[key].push_back(cb_ptr);
    }
}

void mock_sdfs_master::on_append(std::unordered_set<string> const& keys, sdfs::append_callback const& callback) {
    unlocked<callback_state> callbacks = callbacks_lock();
    sdfs::append_callback *cb_ptr = new append_callback(callback);
    callbacks->append_callback_pool.push_back(unique_ptr<sdfs::append_callback>(cb_ptr));
    for (auto const& key : keys) {
        callbacks->append_callbacks[key].push_back(cb_ptr);
    }
}

void mock_sdfs_master::on_get(std::unordered_set<string> const& keys, sdfs::get_callback const& callback) {
    unlocked<callback_state> callbacks = callbacks_lock();
    sdfs::get_callback *cb_ptr = new get_callback(callback);
    callbacks->get_callback_pool.push_back(unique_ptr<sdfs::get_callback>(cb_ptr));
    for (auto const& key : keys) {
        callbacks->get_callbacks[key].push_back(cb_ptr);
    }
}

void mock_sdfs_master::on_del(std::unordered_set<string> const& keys, sdfs::del_callback const& callback) {
    unlocked<callback_state> callbacks = callbacks_lock();
    sdfs::del_callback *cb_ptr = new del_callback(callback);
    callbacks->del_callback_pool.push_back(unique_ptr<sdfs::del_callback>(cb_ptr));
    for (auto const& key : keys) {
        callbacks->del_callbacks[key].push_back(cb_ptr);
    }
}

auto mock_sdfs_master::ls_files(string const& sdfs_dir) -> std::optional<std::vector<string>> {
    return client->ls_files(sdfs_dir);
}

auto mock_sdfs_master::ls_dirs(string const& sdfs_dir) -> std::optional<std::vector<string>> {
    return client->ls_dirs(sdfs_dir);
}

auto mock_sdfs_master::get_metadata(const string &sdfs_path) -> std::optional<sdfs_metadata> {
    return client->get_metadata(sdfs_path);
}

void mock_sdfs_master::wait_transactions() const {
    client->wait_transactions();
}

register_test_service<sdfs_master, mock_sdfs_master> register_mock_sdfs_master;
