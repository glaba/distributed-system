#pragma once

#include "messaging.h"
#include "environment.h"
#include "tcp.h"
#include "locking.h"
#include "serialization.h"
#include "member_list.h"
#include "heartbeater.h"
#include "serialization.h"
#include "configuration.h"
#include "logging.h"

#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <random>
#include <queue>

class messaging_impl;

class conversation_impl : public conversation {
public:
    conversation_impl(std::function<void(std::string const&)> send_fn) : send_fn(send_fn) {}

    // Blocks while sending a message
    void send_raw(uint32_t serializable_id, std::string const& msg);
    // Blocks while waiting for a message, returning the message and the serializable ID of the message
    // Throws an exception if the received message is invalid or if the connection closed
    auto receive_raw() -> std::tuple<std::string, uint32_t>;

    class serializable_id_container : public serializable<serializable_id_container> {
    public:
        serializable_id_container() {}
        serializable_id_container(uint32_t serializable_id) : serializable_id(serializable_id) {}
        uint32_t serializable_id;
    };

private:
    locked<std::queue<std::string>> msg_queue_lock;
    std::condition_variable_any cv_msg;
    std::function<void(std::string const&)> send_fn;

    friend class messaging_impl;
};

class messaging_impl : public messaging, public service_impl<messaging_impl> {
public:
    messaging_impl(environment &env);

    void start();
    void stop();

    void register_conversation_handler(uint32_t serializable_id, std::function<void(conversation&)> handler) {
        (*handlers_lock())[serializable_id] = handler;
    }

    auto start_conversation(std::string host, uint32_t serializable_id, std::string initial_msg) -> std::unique_ptr<conversation>;

    class msg_data : public serializable<msg_data> {
    public:
        msg_data() {}
        msg_data(uint32_t c_id, uint32_t s_id) : conversation_id(c_id), serializable_id(s_id) {}
        uint32_t conversation_id;
        uint32_t serializable_id;
    };

private:
    locked<std::unordered_map<uint32_t, std::function<void(conversation&)>>> handlers_lock;
    locked<std::mt19937> mt;

    struct messaging_state {
        bool running = false;
        // List of newly joined members or members who we have lost connection
        // to that we must connect to
        std::unordered_set<member, member_hash> new_members;
        // List of members with which we have an active connection
        std::unordered_set<member, member_hash> members;

        // Maps from node ID to the tcp_client object or server FD corresponding to that node
        std::unordered_map<uint32_t, std::unique_ptr<tcp_client>> clients;
        std::unordered_map<uint32_t, int> server_fds;
    };
    locked<messaging_state> msg_state_lock;
    std::condition_variable_any cv_state_change;

    locked<std::vector<std::function<void()>>> cleanup_callbacks;

    void handle_message(std::unordered_map<uint32_t, conversation_impl> &conversation_map,
        uint32_t node_id, msg_data data, std::string const& msg);

    void client_thread_fn();
    void server_thread_fn();

    std::unique_ptr<tcp_server> server;

    tcp_factory *tcp_fac;
    configuration *config;
    heartbeater *hb;
    std::unique_ptr<logger> lg;
};
