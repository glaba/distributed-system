#pragma once

#include "member_list.h"
#include "messages.h"
#include "udp.h"
#include "redundant_queue.h"

#include <string>
#include <thread>
#include <mutex>
#include <tuple>
#include <set>
#include <functional>
#include <atomic>

class heartbeater_intf {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual std::vector<member> get_members() = 0;
    virtual member get_successor() = 0;
    virtual void join_group(std::string introducer) = 0;
    virtual void leave_group() = 0;
    virtual uint32_t get_id() = 0;
    virtual bool is_introducer() = 0;
    virtual void lock_new_joins() = 0;
    virtual void unlock_new_joins() = 0;

    // Sets handlers that will be called when membership list changes
    virtual void on_fail(std::function<void(member)>) = 0;
    virtual void on_leave(std::function<void(member)>) = 0;
    virtual void on_join(std::function<void(member)>) = 0;
};

template <bool is_introducer_>
class heartbeater : public heartbeater_intf {
public:
    heartbeater(member_list *mem_list_, logger *lg_, udp_client_svc *udp_client_, 
        udp_server_svc *udp_server_, std::string local_hostname_, uint16_t port_);

    void start();

    void stop();

    // Returns the list of members of the group that this node is aware of
    std::vector<member> get_members();

    // Returns the next node after this one in the membership list
    // If we have not yet joined the group, returns an empty member
    member get_successor();

    // Initiates an async request to join the group by sending a message to the introducer
    void join_group(std::string introducer);

    // Sends a message to peers stating that we are leaving
    void leave_group();

    uint32_t get_id() {
        return our_id;
    }

    bool is_introducer() {
        return is_introducer_;
    }

    // If we are the introducer, prevents any nodes from joining
    void lock_new_joins() {
        nodes_can_join = false;
    }

    // If we are the introducer, allows nodes to join again
    void unlock_new_joins() {
        nodes_can_join = true;
    }

    void on_fail(std::function<void(member)>);
    void on_leave(std::function<void(member)>);
    void on_join(std::function<void(member)>);

private:
    // Client thread function
    void client();

    // Server thread function
    void server();

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
    std::string local_hostname;
    std::atomic<bool> joined_group;

    // Externally provided services for heartbeater to use
    logger *lg;
    udp_client_svc *udp_client;
    udp_server_svc *udp_server;

    // Lists of nodes that have failed / left / joined that we will tell our neighbors
    redundant_queue<uint32_t> failed_nodes_queue;
    redundant_queue<uint32_t> left_nodes_queue;
    redundant_queue<member> joined_nodes_queue;
    // (If we are the introducer), queue of new nodes that should be sent the membership list
    redundant_queue<member> new_nodes_queue;

    // Boolean indicating whether or not new nodes can join (if we are the introducer)d
    std::atomic<bool> nodes_can_join;

    // Set containing all IDs that have ever joined
    std::set<uint32_t> joined_ids;

    // Membership list and mutex protecting membership list access
    member_list *mem_list;
    std::mutex member_list_mutex;

    // Port used for communication with other hosts
    uint16_t port;

    // Handlers that will be called when the membership list changes
    std::vector<std::function<void(member)>> on_fail_handlers;
    std::vector<std::function<void(member)>> on_join_handlers;
    std::vector<std::function<void(member)>> on_leave_handlers;

    // The client and server threads and a boolean used to stop the threads
    std::thread *server_thread, *client_thread;
    std::atomic<bool> running;
};
