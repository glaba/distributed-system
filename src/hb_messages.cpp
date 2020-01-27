#include "hb_messages.h"

namespace hb_message {
    register_serializable<common_data> register_common([] (auto &registry) {
        registry.register_fields(&common_data::type, &common_data::id);
    });
    register_serializable<join_request, common_data, member> register_join_request([] (auto &registry) {
        registry.register_fields(&join_request::common, &join_request::candidate);
    });
    register_serializable<heartbeat, common_data, member> register_heartbeat([] (auto &registry) {
        registry.register_fields(&heartbeat::common, &heartbeat::failed_nodes, &heartbeat::left_nodes, &heartbeat::joined_nodes);
    });
}
