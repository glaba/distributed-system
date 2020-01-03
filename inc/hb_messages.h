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
    void make_join_request(member const& us);
    // Returns true if this message is a join request
    auto is_join_request() const -> bool;
    // Get the member that is requesting to join the group
    auto get_join_request() const -> member;

    // Sets the list of failed nodes to the given list of nodes
    void set_failed_nodes(std::vector<uint32_t> const& nodes);
    // Sets the list of joined nodes to the given list of nodes
    void set_joined_nodes(std::vector<member> const& nodes);
    // Sets the list of left nodes to the given list of nodes
    void set_left_nodes(std::vector<uint32_t> const& nodes);

    // Returns true if the deserialized message is not malformed
    auto is_well_formed() const -> bool;

    // Gets the ID of the node that produced the message
    auto get_id() const -> uint32_t;
    // Gets the list of nodes that failed
    auto get_failed_nodes() const -> std::vector<uint32_t>;
    // Gets the list of nodes that left
    auto get_left_nodes() const -> std::vector<uint32_t>;
    // Gets the list of nodes that joined
    auto get_joined_nodes() const -> std::vector<member>;

    // Serializes the message and returns a string containing the message
    auto serialize() const -> std::string;

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
