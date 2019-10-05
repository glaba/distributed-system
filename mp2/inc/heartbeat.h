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

    // this function will process the information received in a T (table) message and update the member table
    void process_table_msg(std::string msg);

    // this function will process the information received in a J (join) message and update the member table
    void process_join_msg(std::string msg);

    // this function will process the information received in a L (leave) message and update the member table
    void process_leave_msg(std::string msg);

    // this function will construct a message that encodes the important bits of its member list
    std::string construct_table_msg();

    // this function will construct a message that can be used to relay that hostname has left the membership
    std::string construct_leave_msg(std::string hostname);

    // this function will construct a message that can be used to relay that hostname has failed
    std::string construct_fail_msg(std::string hostname);

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
