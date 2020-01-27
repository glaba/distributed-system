#include "election_messages.h"

namespace election_message {
    register_serializable<common_data> register_common([] (auto &registry) {
        registry.register_fields(&common_data::type, &common_data::id, &common_data::uuid);
    });
    register_serializable<introduction, common_data> register_introduction([] (auto &registry) {
        registry.register_fields(&introduction::common, &introduction::master_id);
    });
    register_serializable<election, common_data> register_election([] (auto &registry) {
        registry.register_fields(&election::common, &election::initiator_id, &election::vote_id);
    });
    register_serializable<elected, common_data> register_elected([] (auto &registry) {
        registry.register_fields(&elected::common, &elected::master_id);
    });
    register_serializable<proposal, common_data> register_proposal([] (auto &registry) {
        registry.register_fields(&proposal::common);
    });
}