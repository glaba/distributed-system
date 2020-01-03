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

    auto operator==(const election_message &m) -> bool;

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
    auto is_well_formed() const -> bool {
        return id != 0;
    }

    // Gets the ID of the node that produced the message
    auto get_id() const -> uint32_t {
        return id;
    }
    // Gets the type of the message
    auto get_type() const -> msg_type {
        return type;
    }
    // Getters for the message contents
    auto get_initiator_id() const -> uint32_t {
        return initiator_id;
    }
    auto get_vote_id() const -> uint32_t {
        return vote_id;
    }
    auto get_master_id() const -> uint32_t {
        return master_id;
    }
    auto get_uuid() const -> uint32_t {
        return uuid;
    }

    auto print_type() const -> std::string {
        switch (type) {
            case msg_type::empty: return "EMPTY";
            case msg_type::introduction: return "INTRODUCTION";
            case msg_type::election: return "ELECTION";
            case msg_type::elected: return "ELECTED";
            case msg_type::proposal: return "PROPOSAL";
            default: assert(false && "Memory corruption has occurred and message type is invalid");
        }
    }

    // Serializes the message and returns a string containing the message
    auto serialize() const -> std::string;

private:
    uint32_t id; // The ID of the node that produced the message
    msg_type type;
    uint32_t initiator_id;
    uint32_t vote_id;
    uint32_t master_id;
    // A unique ID for this message such that a node which sees a message with the same uuid more than once
    // will ignore any instances of the message other than the first one received
    uint32_t uuid;
};
