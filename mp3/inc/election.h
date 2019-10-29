#pragma once

#include "member_list.h"
#include "heartbeater.h"

#include <mutex>

class election {
public:
	election(heartbeater_intf *hb_);

	member get_master_node();
private:
	// The heartbeater whose member list we will use to determine the master node
	heartbeater_intf *hb;

	// The current master node (an ID of 0 means there is no master node) and mutex protecting it
	member master_node;
	std::mutex master_node_mutex;
};
