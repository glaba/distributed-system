#pragma once

#include "heartbeater.h"
#include "service.h"
#include "member_list.h"
#include "hb_messages.h"
#include "udp.h"
#include "redundant_queue.h"
#include "environment.h"
#include "configuration.h"

#include <string>
#include <thread>
#include <mutex>
#include <tuple>
#include <set>
#include <functional>
#include <atomic>

class heartbeater_impl : public heartbeater, public service_impl<heartbeater_impl> {
public:
    heartbeater_impl(environment &env);

    void start();
    void stop();
    std::vector<member> get_members();
    member get_member_by_id(uint32_t id);
    member get_successor();
    void run_atomically_with_mem_list(std::function<void()>);
    void join_group(std::string introducer);
    void leave_group();

    uint32_t get_id() {
        return our_id;
    }

    void lock_new_joins() {
        nodes_can_join = false;
    }

    void unlock_new_joins() {
        nodes_can_join = true;
    }

    void on_fail(std::function<void(member)>);
    void on_leave(std::function<void(member)>);
    void on_join(std::function<void(member)>);

private:
    // Fuction that runs the client side code in its own thread
    void client_thread_function();

    // Function that runs the server side code in its own thread
    void server_thread_function();

    // Scans through neighbors and marks those with a heartbeat past the timeout as failed
    void check_for_failed_neighbors();

    // (Should be called only by introducer) Sends pending messages to newly joined nodes
    void send_introducer_msg();

    // Number of times to send each message
    const int message_redundancy = 4;
    // Time between sending heartbeats (in ms)
    const uint64_t heartbeat_interval_ms = 250;
    // Time interval between received heartbeats in which node is marked as failed (in ms)
    const uint64_t timeout_interval_ms = 2000;

    // Information about current host
    uint32_t our_id;
    std::atomic<bool> joined_group;

    // Services that the heartbeater uses
    std::unique_ptr<logger> lg;
    configuration *config;
    std::unique_ptr<udp_client> client;
    std::unique_ptr<udp_server> server;

    // Lists of nodes that have failed / left / joined that we will tell our neighbors
    redundant_queue<uint32_t> failed_nodes_queue;
    redundant_queue<uint32_t> left_nodes_queue;
    redundant_queue<member> joined_nodes_queue;
    // (If we are the introducer), queue of new nodes that should be sent the membership list
    redundant_queue<member> new_nodes_queue;

    // Boolean indicating whether or not new nodes can join (if we are the introducer)
    std::atomic<bool> nodes_can_join;

    // Set containing all IDs that have ever joined
    std::set<uint32_t> joined_ids;

    // Membership list and mutex protecting membership list access
    member_list mem_list;
    std::recursive_mutex member_list_mutex;

    // Handlers that will be called when the membership list changes
    std::vector<std::function<void(member)>> on_fail_handlers;
    std::vector<std::function<void(member)>> on_join_handlers;
    std::vector<std::function<void(member)>> on_leave_handlers;

    // The client and server threads and a boolean used to stop the threads
    std::unique_ptr<std::thread> server_thread, client_thread;
    std::atomic<bool> running;
};
