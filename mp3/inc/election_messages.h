#pragma once

#include <string>
#include <memory>
#include <cassert>

class election_message {
public:
    enum msg_type {
        empty, introduction, election, elected, proposal
    };

    // Creates a message from a buffer
    election_message(char *buf, unsigned length);

    // Creates a message that will be empty by default
    election_message(uint32_t id_, uint32_t uuid_) : id(id_), type(), uuid(uuid_) {}

    bool operator==(const election_message &m);

    // Sets the message to be an INTRODUCTION message
    void set_type_introduction(uint32_t master_id_) {
        type = msg_type::introduction;
        master_id = master_id_;
    }

    // Sets the message to be an ELECTION message
    void set_type_election(uint32_t initiator_id_, uint32_t vote_id_) {
        type = msg_type::election;
        initiator_id = initiator_id_;
        vote_id = vote_id_;
    }

    // Sets the message to be an ELECTED message
    void set_type_elected(uint32_t master_id_) {
        type = msg_type::elected;
        master_id = master_id_;
    }

    // Sets the message to be a PROPOSAL message
    void set_type_proposal() {
        type = msg_type::proposal;
    }

    // Returns true if the deserialized message is not malformed
    bool is_well_formed() {
        return malformed_reason == "";
    }
    // Returns the reason why the message is malformed
    std::string why_malformed() {
        return malformed_reason;
    }

    // Gets the ID of the node that produced the message
    uint32_t get_id() {
        return id;
    }
    // Gets the type of the message
    msg_type get_type() {
        return type;
    }
    // Getters for the message contents
    uint32_t get_initiator_id() {
        return initiator_id;
    }
    uint32_t get_vote_id() {
        return vote_id;
    }
    uint32_t get_master_id() {
        return master_id;
    }
    uint32_t get_uuid() {
        return uuid;
    }

    std::string print_type() {
        switch (type) {
            case msg_type::empty: return "EMPTY";
            case msg_type::introduction: return "INTRODUCTION";
            case msg_type::election: return "ELECTION";
            case msg_type::elected: return "ELECTED";
            case msg_type::proposal: return "PROPOSAL";
            default: assert(false && "Memory corruption has occurred and message type is invalid");
        }
    }

    // Serializes the message and returns a buffer containing the message, along with the length
    std::unique_ptr<char[]> serialize(unsigned &length);

private:
    std::string malformed_reason = "";
    uint32_t id; // The ID of the node that produced the message
    msg_type type;
    uint32_t initiator_id;
    uint32_t vote_id;
    uint32_t master_id;
    // A unique ID for this message such that a node which sees a message with the same uuid more than once
    // will ignore any instances of the message other than the first one received
    uint32_t uuid;
};
