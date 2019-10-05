#pragma once

#include "member_list.h"

#include <cstdint>
#include <string>
#include <thread>

class heartbeater {
public:
    // Constructor to be used if the current machine is the introducer
    heartbeater(member_list mem_list_, std::string local_hostname_, uint16_t port_);
    // Constructor to be used if the current machine is not the introducer
    heartbeater(member_list mem_list_, std::string local_hostname_, std::string introducer_, uint16_t port_);

    void start();

    /* Client side functions */
    // Client thread function
    void client();
    // Initiates an async request to join the group by sending a message to the introducer
    void join_group(std::string introducer);
    // Returns true if we have joined a group (i.e. the membership list is non-empty)
    bool has_joined();

    /* Server side functions */
    // Server thread function
    void server();
private:
    member_list mem_list;
    std::string local_hostname;
    std::string introducer;
    bool is_introducer;
    uint16_t port;
};
