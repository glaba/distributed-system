#pragma once

#include "serialization.h"

#include <string>
#include <memory>
#include <cassert>

namespace election_message {
    enum class msg_type {
        introduction, election, elected, proposal
    };

    // Data that is common between all message types
    class common_data : public serializable<common_data> {
    public:
        common_data() = default;
        common_data(uint32_t type, uint32_t id, uint32_t uuid) : type(type), id(id), uuid(uuid) {}
        // Currently manually set and manually deserialized to determine which message type it is
        // TODO: remove this replace with messaging so that this is automatic
        uint32_t type;
        // The ID of the node that produced the message
        uint32_t id;
        // A unique ID for the message so that duplicate messages can be ignored
        uint32_t uuid;
    };

    class introduction : public serializable<introduction, common_data> {
    public:
        introduction() = default;
        introduction(uint32_t id, uint32_t uuid, uint32_t master_id_)
            : common(static_cast<uint32_t>(msg_type::introduction), id, uuid), master_id(master_id_) {}

        common_data common;
        // The ID of the master node
        uint32_t master_id;
    };

    class election : public serializable<election, common_data> {
    public:
        election() = default;
        election(uint32_t id, uint32_t uuid, uint32_t initiator_id_, uint32_t vote_id_)
            : common(static_cast<uint32_t>(msg_type::election), id, uuid), initiator_id(initiator_id_), vote_id(vote_id_) {}

        common_data common;
        // The ID of the node which initiated the election
        uint32_t initiator_id;
        // The ID of the node being voted for
        uint32_t vote_id;
    };

    class elected : public serializable<elected, common_data> {
    public:
        elected() = default;
        elected(uint32_t id, uint32_t uuid, uint32_t master_id_)
            : common(static_cast<uint32_t>(msg_type::elected), id, uuid), master_id(master_id_) {}

        common_data common;
        // The ID of the node which is being confirmed as master node
        uint32_t master_id;
    };

    class proposal : public serializable<proposal, common_data> {
    public:
        proposal() = default;
        proposal(uint32_t id, uint32_t uuid)
            : common(static_cast<uint32_t>(msg_type::proposal), id, uuid) {}

        common_data common;
    };
}
