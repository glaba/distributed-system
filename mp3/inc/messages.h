#pragma once

#include "member_list.h"

#include <vector>

class message {
public:
    // Creates a message from a buffer (safe)
    message(char *buf, unsigned length);

    // Creates an message that will be empty by default
    message(uint32_t id_) : id(id_) {}

    // Sets the list of failed nodes to the given list of nodes
    void set_failed_nodes(std::vector<uint32_t> nodes);
    // Sets the list of joined nodes to the given list of nodes
    void set_joined_nodes(std::vector<member> nodes);
    // Sets the list of left nodes to the given list of nodes
    void set_left_nodes(std::vector<uint32_t> nodes);

    // Returns true if the deserialized message is not malformed
    bool is_well_formed();
    // Returns the reason why the message is malformed
    std::string why_malformed() {
        return malformed_reason;
    }

    // Gets the ID of the node that produced the message
    uint32_t get_id();
    // Gets the list of nodes that failed
    std::vector<uint32_t> get_failed_nodes();
    // Gets the list of nodes that left
    std::vector<uint32_t> get_left_nodes();
    // Gets the list of nodes that joined
    std::vector<member> get_joined_nodes();

    // Serializes the message and returns a buffer containing the message, along with the length
    char *serialize(unsigned &length);

private:
    std::string malformed_reason = "";
    uint32_t id; // The ID of the node that produced the message
    std::vector<uint32_t> failed_nodes;
    std::vector<uint32_t> left_nodes;
    std::vector<member> joined_nodes;
};
