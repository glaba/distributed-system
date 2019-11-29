#include "hb_messages.h"
#include "serialization.h"

#include <iostream>
#include <cassert>

using std::unique_ptr;

// Creates a message from a buffer
hb_message::hb_message(char *buf_, unsigned length_) {
    deserializer des(buf_, length_);

    try {
        id = des.get_int();

        int msg_type = des.get_int();
        if (msg_type == JOIN_REQUEST_ID) {
            join_request = true;

            join_request_member.hostname = des.get_string();
            join_request_member.id = des.get_int();

        } else if (msg_type == NORMAL_HEARTBEAT_ID) {
            join_request = false;

            uint32_t num_failed = des.get_int();
            for (unsigned i = 0; i < num_failed; i++) {
                failed_nodes.push_back(des.get_int());
            }

            uint32_t num_left = des.get_int();
            for (unsigned i = 0; i < num_left; i++) {
                left_nodes.push_back(des.get_int());
            }

            uint32_t num_joined = des.get_int();
            for (unsigned i = 0; i < num_joined; i++) {
                joined_nodes.push_back(member());
                joined_nodes[i].hostname = des.get_string();
                joined_nodes[i].id = des.get_int();
            }

        } else {
            throw "Invalid message type";
        }

        des.done();
    } catch (...) {
        id = 0;
        join_request = false;
        failed_nodes.clear();
        left_nodes.clear();
        joined_nodes.clear();
    }
}

// Serializes the message and returns a string containing the message
std::string hb_message::serialize() {
    serializer ser;

    ser.add_field(id);

    if (join_request) {
        ser.add_field(JOIN_REQUEST_ID);

        ser.add_field(join_request_member.hostname);
        ser.add_field(join_request_member.id);
    } else {
        ser.add_field(NORMAL_HEARTBEAT_ID);

        ser.add_field(failed_nodes.size());
        for (uint32_t failed_id : failed_nodes) {
            ser.add_field(failed_id);
        }

        ser.add_field(left_nodes.size());
        for (uint32_t left_id : left_nodes) {
            ser.add_field(left_id);
        }

        ser.add_field(joined_nodes.size());
        for (member m : joined_nodes) {
            ser.add_field(m.hostname);
            ser.add_field(m.id);
        }
    }

    return ser.serialize();
}

// Makes this message a join request, as opposed to a regular heartbeat message
void hb_message::make_join_request(member us) {
    join_request = true;
    join_request_member = us;
}

// Returns true if this message is a join request
bool hb_message::is_join_request() {
    return join_request;
}

// Get the member that is requesting to join the group
member hb_message::get_join_request() {
    return join_request_member;
}

// Sets the list of failed nodes to the given list of nodes
void hb_message::set_failed_nodes(std::vector<uint32_t> nodes) {
    failed_nodes = nodes;
}

// Sets the list of joined nodes to the given list of nodes
void hb_message::set_joined_nodes(std::vector<member> nodes) {
    joined_nodes = nodes;
}

// Sets the list of left nodes to the given list of nodes
void hb_message::set_left_nodes(std::vector<uint32_t> nodes) {
    left_nodes = nodes;
}

// Returns true if the deserialized message is not malformed
bool hb_message::is_well_formed() {
    return id != 0;
}

// Gets the ID of the node that produced the message
uint32_t hb_message::get_id() {
    return id;
}

// Gets the list of nodes that failed
std::vector<uint32_t> hb_message::get_failed_nodes() {
    return failed_nodes;
}

// Gets the list of nodes that left
std::vector<uint32_t> hb_message::get_left_nodes() {
    return left_nodes;
}

// Gets the list of nodes that joined
std::vector<member> hb_message::get_joined_nodes() {
    return joined_nodes;
}
