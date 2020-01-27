#pragma once

#include "member_list.h"
#include "serialization.h"

#include <vector>

namespace hb_message {
    enum class msg_type {
        join_request, heartbeat
    };

    class common_data : public serializable<common_data> {
    public:
        common_data() = default;
        common_data(uint32_t type, uint32_t id) : type(type), id(id) {}

        // TODO: remove this in favor of messaging
        uint32_t type;
        // ID of the node that produced the message
        uint32_t id;
    };

    class join_request : public serializable<join_request, common_data, member> {
    public:
        join_request() = default;
        join_request(uint32_t id, member candidate)
            : common(static_cast<uint32_t>(msg_type::join_request), id), candidate(candidate) {}

        common_data common;
        // Member that wants to join the group
        member candidate;
    };

    class heartbeat : public serializable<heartbeat, common_data, member> {
    public:
        heartbeat() = default;
        heartbeat(uint32_t id, std::vector<uint32_t> const& failed_nodes, std::vector<uint32_t> const& left_nodes, std::vector<member> const& joined_nodes)
            : common(static_cast<uint32_t>(msg_type::heartbeat), id), failed_nodes(failed_nodes),
              left_nodes(left_nodes), joined_nodes(joined_nodes) {}

        common_data common;
        // List of nodes recently detected as failed
        std::vector<uint32_t> failed_nodes;
        // List of nodes which recently notified as leaving
        std::vector<uint32_t> left_nodes;
        // List of nodes which recently joined
        std::vector<member> joined_nodes;
    };
}
