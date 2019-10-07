#pragma once

#include "member_list.h"
#include "utils.h"

#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <tuple>

class heartbeater {
public:
    // Constructor to be used if the current machine is the introducer
    heartbeater(member_list mem_list_, logger *lg_, udp_client_svc *udp_client_, udp_server_svc *udp_server_,
        std::string local_hostname_, uint16_t port_);

    // Constructor to be used if the current machine is not the introducer
    heartbeater(member_list mem_list_, logger *lg_, udp_client_svc *udp_client_, udp_server_svc *udp_server_,
        std::string local_hostname_, std::string introducer_, uint16_t port_);

    void start();

    /* Client side functions */
    // Client thread function
    void client();
    // Initiates an async request to join the group by sending a message to the introducer
    void join_group(std::string introducer);
    // Returns true if we have joined a group (i.e. the membership list is non-empty)
    bool has_joined();
    // Creates a message that represents the provided set of failed / left / joined nodes
    char *construct_msg(std::vector<uint32_t> failed_nodes, std::vector<uint32_t> left_nodes, std::vector<member> joined_nodes, unsigned *length);

    /* Server side functions */
    // Server thread function
    void server();
    // Processes a fail message (L), updates the member table, and returns the number of bytes consumed
    unsigned process_fail_msg(char *buf);
    // Processes a leave message (L), updates the member table, and returns the number of bytes consumed
    unsigned process_leave_msg(char *buf);
    // Processes a join message (J), updates the member table, and returns the number of bytes consumed
    unsigned process_join_msg(char *buf);

    void add_fail_msg_to_list(uint32_t id);
    void add_leave_msg_to_list(uint32_t id);
    void add_join_msg_to_list(uint32_t id);

    // number of times to send each message
    static int message_redundancy;
    // time between sending heartbeats (in ms)
    static uint64_t heartbeat_interval_ms;
    // time interval between received heartbeats in which node is marked as failed (in ms)
    static uint64_t timeout_interval_ms;
private:
    int our_id;
    member_list mem_list;
    logger *lg;
    udp_client_svc *udp_client; // Separated UDP client service for easy mocking
    udp_server_svc *udp_server; // Separated UDP server service for easy mocking
    std::string local_hostname;
    std::string introducer;
    std::vector<std::tuple<uint32_t, int>> failed_nodes_counts;
    std::vector<std::tuple<uint32_t, int>> left_nodes_counts;
    std::vector<std::tuple<member, int>> joined_nodes_counts;
    // Tuple of <membership table LLJ... message, message length, new node ID, TTL> for all new nodes if we are introducer
    std::vector<std::tuple<char*, unsigned, uint32_t, int>> new_node_introduction_counts;
    bool is_introducer;
    uint16_t port;
    std::mutex member_list_mutex;
};
