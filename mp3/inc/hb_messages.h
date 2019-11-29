#pragma once

#include "member_list.h"

#include <vector>
#include <memory>

class hb_message {
public:
    // Creates a message from a buffer (safe)
    hb_message(char *buf, unsigned length);

    // Creates an message that will be empty by default
    hb_message(uint32_t id_) : id(id_) {}

    // Makes this message a join request, as opposed to a regular heartbeat message
    void make_join_request(member us);
    // Returns true if this message is a join request
    bool is_join_request();
    // Get the member that is requesting to join the group
    member get_join_request();

    // Sets the list of failed nodes to the given list of nodes
    void set_failed_nodes(std::vector<uint32_t> nodes);
    // Sets the list of joined nodes to the given list of nodes
    void set_joined_nodes(std::vector<member> nodes);
    // Sets the list of left nodes to the given list of nodes
    void set_left_nodes(std::vector<uint32_t> nodes);

    // Returns true if the deserialized message is not malformed
    bool is_well_formed();

    // Gets the ID of the node that produced the message
    uint32_t get_id();
    // Gets the list of nodes that failed
    std::vector<uint32_t> get_failed_nodes();
    // Gets the list of nodes that left
    std::vector<uint32_t> get_left_nodes();
    // Gets the list of nodes that joined
    std::vector<member> get_joined_nodes();

    // Serializes the message and returns a string containing the message
    std::string serialize();

private:
    const int JOIN_REQUEST_ID = 0;
    const int NORMAL_HEARTBEAT_ID = 1;

    bool join_request = false;
    member join_request_member; // The member that is requesting to join the group
    uint32_t id; // The ID of the node that produced the message
    std::vector<uint32_t> failed_nodes;
    std::vector<uint32_t> left_nodes;
    std::vector<member> joined_nodes;
};
