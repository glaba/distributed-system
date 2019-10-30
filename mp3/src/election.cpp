#include "election.h"

#include <functional>
#include <chrono>
#include <cassert>
#include <random>
#include <string>

election::election(heartbeater_intf *hb_, logger *lg_, member master_node_) : hb(hb_), lg(lg_), master_node(master_node_) {
	// master_node.id = 0;
	state = election_state::normal;
	state_changed = false;
}

void election::start() {
	running = true;
	timer_on = false;
	election_thread = new std::thread([this] {
		run_election();
	});
	election_thread->detach();

	timer_thread = new std::thread([this] {
		update_timer();
	});
	timer_thread->detach();

	std::function<void(member)> callback = [this](member m) {
		{ // Atomic block for accessing the state
			std::lock_guard<std::mutex> guard(state_mutex);

			if (state != election_state::normal) {
				return;
			}
		}

		if (m.id == master_node.id) {
			lg->log("Master node failed!");
			transition(election_state::election_wait);
		}
	};

	hb->on_fail(callback);
	hb->on_leave(callback);
}

std::string election::print_state() {
	switch (state) {
		case normal: return "NORMAL";
		case election_wait: return "ELECTION_WAIT";
		case election_init: return "ELECTION_INIT";
		case electing: return "ELECTING";
		case elected: return "ELECTED";
		case failure: return "FAILURE";
		default: return "";
	}
}

void election::transition(election_state s) {
	next_state = s;
	state_changed = true;
}

void election::run_election() {
	while (running.load()) {
		// Perform the post-transition operation for the state we transitioned into
		if (state_changed.load()) {
			std::lock_guard<std::mutex> guard(state_mutex);

			state = next_state;
			state_changed = false;
			lg->log("State changed to " + print_state());

			// We should already have the state_mutex, so we can safely use state
			switch (state) {
				case election_wait: {
					hb->lock_new_joins();

					// Start timer with a range so that not all nodes send initiation messages
					start_timer(MEMBER_LIST_STABILIZATION_TIME + (std::rand() % MEMBER_LIST_STABILIZATION_RANGE));
					lg->log("Waiting for membership list to stabilize or for an election to be initiated");
					break;
				}
				case election_init: {
					std::string successor = hb->get_successor().hostname;

					// TODO: implement message serialization and TCP
					// send(successor, {type: ELECTION, initiator ID: hb->get_id(), vote ID: hb->get_id()});
					transition(election_state::electing);
					lg->log("Sent election initiation message to successor at " + successor);
					break;
				}
				case electing: {
					// Set timer at which point the election times out and we restart the election
					start_timer(hb->get_members().size() * ELECTION_TIMEOUT_TIME);
					lg->log("Election has begun, timer waiting for election to time out");
					break;
				}
				case elected: {
					std::string successor = hb->get_successor().hostname;

					// TODO: implement message serializaiton and TCP
					// send(successor, {type: ELECTED, master ID: our ID})
					lg->log("We have been selected as the new master node. Sending elected message to successor at " + successor);
					break;
				}
				case failure: {
					assert(false && "Either election algorithm is flawed or severe network partition has occurred!");
					break;
				}
				case normal:
				default:
					break;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void election::start_timer(uint32_t time) {
	std::lock_guard<std::mutex> guard(timer_mutex);

	timer_on = true;
	timer = time;
	prev_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void election::update_timer() {
	for (; running.load(); std::this_thread::sleep_for(std::chrono::milliseconds(100))) {
		{ // Atomic block for updating timer variables
			std::lock_guard<std::mutex> guard(timer_mutex);

			if (timer_on) {
				// Update timer without letting it overflow (since it is unsigned)
				uint32_t cur_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
				uint32_t diff = cur_time - prev_time;
				prev_time = cur_time;
				timer = (timer > diff) ? (timer - diff) : 0;

				if (timer != 0) {
					continue;
				} else {
					// Now that the timer has expired, turn the timer off before we perform the associated action
					timer_on = false;
				}
			} else {
				continue;
			}
		}

		{ // Atomic block for accessing the state
			std::lock_guard<std::mutex> guard(state_mutex);

			switch (state) {
				case election_wait: {
					transition(election_state::election_init);
					lg->log("Membership list has stabilized, sending election initiation message");
					break;
				}
				case electing: {
					transition(election_state::election_wait);
					lg->log("Election has timed out, restarting election");
					break;
				}
				case normal:
				case election_init:
				case elected:
				case failure:
				default:
					break;
			}
		}
	}
}

election::~election() {
	running = false;
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	delete election_thread;
	delete timer_thread;
}

member election::get_master_node(bool *succeeded) {
	std::lock_guard<std::mutex> guard(state_mutex);

	*succeeded = (state == normal);
	return master_node;
}