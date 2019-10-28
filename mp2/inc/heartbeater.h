#pragma once

#include "member_list.h"
#include "messages.h"
#include "utils.h"
#include "redundant_queue.h"

#include <string>
#include <thread>
#include <mutex>
#include <tuple>
#include <set>
#include <atomic>

class heartbeater_intf {
public:
    virtual void start() = 0;
    virtual std::vector<member> get_members() = 0;
    virtual void join_group(std::string introducer) = 0;
    virtual void leave_group() = 0;
    virtual uint32_t get_id() = 0;
};

template <bool is_introducer>
class heartbeater : public heartbeater_intf {
public:
    heartbeater(member_list *mem_list_, logger *lg_, udp_client_svc *udp_client_, 
        udp_server_svc *udp_server_, std::string local_hostname_, uint16_t port_);

    void start();

    // Returns the list of members of the group that this node is aware of
    std::vector<member> get_members();

    // Initiates an async request to join the group by sending a message to the introducer
    void join_group(std::string introducer);

    // Sends a message to peers stating that we are leaving
    void leave_group();

    uint32_t get_id() {
        return our_id;
    }

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

    // Set containing all IDs that have ever joined
    std::set<uint32_t> joined_ids;

    // Membership list and mutex protecting membership list access
    member_list *mem_list;
    std::mutex member_list_mutex;

    // Port used for communication with other hosts
    uint16_t port;
};
