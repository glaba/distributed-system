#pragma once

#include "member_list.h"
#include "utils.h"

#include <cstdint>
#include <string>
#include <thread>
#include <mutex>

class heartbeater {
public:
    // Constructor to be used if the current machine is the introducer
    heartbeater(member_list mem_list_, udp_client_svc *udp_client_, udp_server_svc *udp_server_,
        std::string local_hostname_, uint16_t port_);

    // Constructor to be used if the current machine is not the introducer
    heartbeater(member_list mem_list_, udp_client_svc *udp_client_, udp_server_svc *udp_server_,
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
private:
    int our_id;
    member_list mem_list;
    udp_client_svc *udp_client; // Separated UDP client service for easy mocking
    udp_server_svc *udp_server; // Separated UDP server service for easy mocking
    std::string local_hostname;
    std::string introducer;
    bool is_introducer;
    uint16_t port;
    // TODO: make sure this mutex is properly used between all threads
    std::mutex member_list_mutex;
};
