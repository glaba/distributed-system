#include "election.h"

#include <functional>

election::election(heartbeater_intf *hb_) : hb(hb_) {
	master_node.id = 0;

	std::function<void(member)> callback = [this](member m) {
		std::lock_guard<std::mutex> guard(master_node_mutex);

		if (m.id == master_node.id) {
			// Begin election
		}
	};

	hb->on_fail(callback);
	hb->on_leave(callback);
}

member election::get_master_node() {
	return master_node;
}