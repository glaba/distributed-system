#include "election.h"
#include "election.hpp"
#include "environment.h"
#include "serialization.h"

#include <functional>
#include <chrono>
#include <cassert>
#include <random>
#include <string>
#include <algorithm>

using std::unique_ptr;
using std::string;
using std::optional;

election_impl::election_impl(environment &env)
    : hb(env.get<heartbeater>()),
      lg(env.get<logger_factory>()->get_logger("election")),
      client(env.get<udp_factory>()->get_udp_client()),
      server(env.get<udp_factory>()->get_udp_server()),
      config(env.get<configuration>()), running(false)
{
    unlocked<election_state> el_state = el_state_lock();
    if (config->is_first_node()) {
        // Set ourselves as the master node
        el_state->master_node = hb->get_member_by_id(hb->get_id());
        el_state->state = normal;
    } else {
        el_state->master_node.id = 0;
        el_state->state = no_master;
    }
}

// Starts keeping track of the master node and running elections
void election_impl::start() {
    if (running.load()) {
        return;
    }

    hb->start();
    lg->info("Starting election");

    running = true;
    tm_state_lock()->timer_on = false;

    // Initialize RNG with a slight delay so that unit tests have different seeds
    el_state_lock()->mt = std::mt19937(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    std::thread timer_thread([&] {
        update_timer();
    });
    timer_thread.detach();

    std::thread server_thread([&] {
        server_thread_function();
    });
    server_thread.detach();

    std::thread client_thread([&] {
        client_thread_function();
    });
    client_thread.detach();

    std::function<void(member const&)> callback = [this] (member const& m) {
        unlocked<election_state> el_state = el_state_lock();

        // If we don't know who the master node is, we cannot participate in election since we don't know if failed node was master
        if (el_state->state == no_master) {
            assert(false && "Did not receive master node introduction message at all OR in time for election");
        }

        if (el_state->state != normal) {
            return;
        }

        if (m.id == el_state->master_node.id) {
            lg->info("Master node failed! Beginning election process");
            el_state->highest_initiator_id = 0;
            transition(normal, election_wait);
        }
    };

    hb->on_fail(callback);
    hb->on_leave(callback);

    // If we are the master node, tell new nodes that we are the master node
    std::function<void(member)> join_callback = [this](member m) {
        unlocked<election_state> el_state = el_state_lock();

        if (el_state->state == normal && el_state->master_node.id == hb->get_id()) {
            lg->debug("Telling node at " + m.hostname + " that we are the master node");

            // Tell the new node we are the master node
            auto msg = election_message::introduction(hb->get_id(), el_state->mt(), el_state->master_node.id);
            enqueue_message(m.hostname, msg, el_state);
        }

        // If the master node is down, the new node will in state NO_MASTER, and will
        // simply participate in the upcoming elections as a normal node
    };

    hb->on_join(join_callback);
}

// Stops all threads and election logic (may leave master_node in an invalid state)
void election_impl::stop() {
    if (!running.load()) {
        return;
    }

    lg->info("Stopping election");

    server->stop_server();
    running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    hb->stop();
}

// Debugging function to print a string value for the enum
auto election_impl::print_state(election_state_enum const& s) -> string {
    switch (s) {
        case no_master: return "NO_MASTER";
        case normal: return "NORMAL";
        case election_wait: return "ELECTION_WAIT";
        case election_init: return "ELECTION_INIT";
        case electing: return "ELECTING";
        case elected: return "ELECTED";
        default: return "";
    }
}

// Transitions the current state to the given state
void election_impl::transition(election_state_enum origin_state, election_state_enum dest_state) {
    unlocked<election_state> el_state = el_state_lock();

    if (el_state->state != origin_state) {
        assert(false && "Incorrect origin_state for transition");
    }

    // Cancel the timer if it was already running
    stop_timer();

    el_state->state = dest_state;
    lg->debug("Election state transition " + print_state(origin_state) + " -> " + print_state(el_state->state));

    switch (dest_state) {
        case election_wait: {
            hb->lock_new_joins();

            // Start timer with a range so that not all nodes send initiation messages
            start_timer(MEMBER_LIST_STABILIZATION_TIME + (std::rand() % MEMBER_LIST_STABILIZATION_RANGE));
            lg->debug("Waiting for membership list to stabilize or for an election to be initiated");
            break;
        }
        case election_init: {
            // Send an election initiation message to our successor
            auto msg = election_message::election(hb->get_id(), el_state->mt(), hb->get_id(), hb->get_id());
            enqueue_message(hb->get_successor().hostname, msg, el_state);

            el_state->highest_initiator_id = hb->get_id();
            transition(election_init, electing);
            break;
        }
        case electing: {
            // Set timer at which point the election times out and we restart the election
            start_timer(ELECTION_TIMEOUT_TIME);
            lg->debug("Election has begun, timer waiting for election to time out");
            break;
        }
        case elected: {
            // Send a message to the successor stating that we have been elected
            auto msg = election_message::elected(hb->get_id(), el_state->mt(), hb->get_id());
            enqueue_message(hb->get_successor().hostname, msg, el_state);
            lg->debug("We have been nominated as the new master node");

            // Set a timer in case the ELECTED message gets lost in the ring
            start_timer(ELECTED_TIMEOUT_TIME);
            lg->debug("Timer set to wait for election to time out");
            break;
        }
        case normal: {
            // Once we transition back into normal state, new nodes can join again
            hb->unlock_new_joins();
            break;
        }
        case no_master: assert(false && "We should never transition into state NO_MASTER");
        default:
            break;
    }
}

// Starts the timer to run for the given number of milliseconds
void election_impl::start_timer(uint32_t time) {
    unlocked<timer_state> tm_state = tm_state_lock();

    tm_state->timer_on = true;
    tm_state->timer = time;
    tm_state->prev_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Stops the timer
void election_impl::stop_timer() {
    unlocked<timer_state> tm_state = tm_state_lock();

    tm_state->timer_on = false;
    tm_state->timer = 0;
}

// Updates the timer and performs the appropriate action in the state machine if the timer reaches 0
void election_impl::update_timer() {
    uint64_t elapsed_time = 0;
    for (; running.load(); std::this_thread::sleep_for(std::chrono::milliseconds(100))) {
        unlocked<election_state> el_state = el_state_lock();
        unlocked<timer_state> tm_state = tm_state_lock();

        if (tm_state->timer_on) {
            // Update timer without letting it overflow (since it is unsigned)
            uint32_t cur_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            uint32_t diff = cur_time - tm_state->prev_time;
            tm_state->prev_time = cur_time;
            tm_state->timer = (tm_state->timer > diff) ? (tm_state->timer - diff) : 0;

            if (tm_state->timer != 0) {
                continue;
            } else {
                // Now that the timer has expired, turn the timer off before we perform the associated action
                tm_state->timer_on = false;
            }
        } else {
            continue;
        }

        switch (el_state->state) {
            case election_wait: {
                lg->debug("Membership list has stabilized, sending election initiation message");
                transition(election_wait, election_init);
                break;
            }
            case electing:
            case elected: {
                el_state->highest_initiator_id = 0;
                lg->info("Election has timed out, restarting election by sending PROPOSAL message");

                // Create a proposal message to send to the next node to start a new election
                auto msg = election_message::proposal(hb->get_id(), el_state->mt());
                enqueue_message(hb->get_successor().hostname, msg, el_state);

                // Transition to a waiting state while the proposal message makes the rounds and
                //  the membership list stabilizes
                transition(el_state->state, election_wait);
                break;
            }
            case normal:
            case election_init:
            case no_master:
                assert(false && "These states do not have timers associated with them");
            default:
                break;
        }

        // Update seen_message_ids if 1 minute has passed
        uint64_t new_time = elapsed_time + 100;
        if (new_time / 60000 > elapsed_time / 60000) {
            el_state->seen_message_ids.pop();
        }
        elapsed_time = new_time;
    }
}

// Keeps track of the highest initiator ID seen for ELECTION messages so far
void election_impl::add_to_cache(election_message::election const& msg, unlocked<election_state> const& el_state) {
    if (el_state->highest_initiator_id < msg.initiator_id) {
        el_state->highest_initiator_id = msg.initiator_id;
    }
}

// Passes on an ELECTION message as defined by the protocol
void election_impl::propagate(election_message::election const& msg, unlocked<election_state> const& el_state) {
    if (msg.initiator_id >= el_state->highest_initiator_id) {
        assert(msg.vote_id != hb->get_id());

        // If we are the initiator, change the UUID of the message to indicate that
        //  we are beginning the second cycle through the ring
        uint32_t uuid = (msg.initiator_id == hb->get_id()) ? el_state->mt() : msg.common.uuid;
        // Update the vote ID to max(current ID, our ID)
        uint32_t vote_id = std::max(msg.vote_id, hb->get_id());

        election_message::election new_msg(hb->get_id(), uuid, msg.initiator_id, vote_id);
        enqueue_message(hb->get_successor().hostname, new_msg, el_state);
    }
}

// Enqueues a message into the message_queue and prints debug information
void election_impl::enqueue_message(string const& dest, election_message::introduction const& msg, unlocked<election_state> const& el_state) {
    lg->debug("Informing new node at " + dest + " that master node is " + hb->get_member_by_id(msg.master_id).hostname);
    el_state->message_queue.push({dest, msg.serialize()}, introduction_message_redundancy);
}

void election_impl::enqueue_message(string const& dest, election_message::election const& msg, unlocked<election_state> const& el_state) {
    lg->debug("Sending ELECTION message with initiator ID " + std::to_string(msg.initiator_id) +
        " and vote ID " + std::to_string(msg.vote_id) + " to node at " + dest);
    el_state->message_queue.push({dest, msg.serialize()}, message_redundancy);
}

void election_impl::enqueue_message(string const& dest, election_message::elected const& msg, unlocked<election_state> const& el_state) {
    lg->debug("Sending ELECTED message with master ID " + std::to_string(msg.master_id) + " to node at " + dest);
    el_state->message_queue.push({dest, msg.serialize()}, message_redundancy);
}

void election_impl::enqueue_message(string const& dest, election_message::proposal const& msg, unlocked<election_state> const& el_state) {
    lg->debug("Sending PROPOSAL message with UUID " + std::to_string(msg.common.uuid) + " to node at " + dest);
    el_state->message_queue.push({dest, msg.serialize()}, message_redundancy);
}

template <typename Msg>
auto election_impl::filter_msg(unlocked<election_state> const& el_state, char const* buf, int size) -> optional<Msg> {
    Msg msg;
    if (!msg.deserialize_from(string(buf, size))) {
        lg->debug("Received malformed election message!");
        return std::nullopt;
    }

    // Make sure that the message originates from within our group
    if (hb->get_member_by_id(msg.common.id).id != msg.common.id) {
        lg->trace("Ignoring election message from " + std::to_string(msg.common.id) + " because they are not in the group");
        return std::nullopt;
    }

    // Check that we have not seen a message with this ID yet
    std::vector<uint32_t> const& seen_ids = el_state->seen_message_ids.peek();
    if (std::find(seen_ids.begin(), seen_ids.end(), msg.common.uuid) != seen_ids.end()) {
        // Ignore the message
        return std::nullopt;
    }
    // Mark this message ID as seen
    el_state->seen_message_ids.push(msg.common.uuid, seen_ids_ttl);
    return msg;
}

void election_impl::process_introduction_msg(unlocked<election_state> const& el_state, election_message::introduction const& msg) {
    hb->run_atomically_with_mem_list([&] () mutable {
        // Check if we are in the state no_master
        if (el_state->state == no_master) {
            // Check if the master node is in our membership list
            if (hb->get_member_by_id(msg.master_id).id == msg.master_id) {
                el_state->master_node = hb->get_member_by_id(msg.master_id);

                lg->info("Received master node at " + el_state->master_node.hostname + " with id " +
                    std::to_string(el_state->master_node.id));

                transition(no_master, normal);
            }
        }
    });
}

void election_impl::process_election_msg(unlocked<election_state> const& el_state, election_message::election const& msg) {
    switch (el_state->state) {
        case no_master: // If we joined after the master node failed, just participate in the elections as normal
        case election_wait: {
            add_to_cache(msg, el_state);
            propagate(msg, el_state);
            lg->debug("Received election message from " + hb->get_member_by_id(msg.common.id).hostname +
                " with initiator ID " + std::to_string(msg.initiator_id) +
                " and vote ID " + std::to_string(msg.vote_id));
            transition(election_wait, electing);
            break;
        }
        case electing: {
            if (msg.vote_id == hb->get_id()) {
                lg->debug("Received election message with our ID from " + hb->get_member_by_id(msg.common.id).hostname);

                transition(electing, elected);
            } else {
                add_to_cache(msg, el_state);
                propagate(msg, el_state);
                lg->debug("Received election message from " + hb->get_member_by_id(msg.common.id).hostname +
                    " with initiator ID " + std::to_string(msg.initiator_id) +
                    " and vote ID " + std::to_string(msg.vote_id));
            }
            break;
        }
        case elected: {
            // Make sure the ELECTION message is nominating a node other than us
            if (msg.common.id == hb->get_id()) {
                assert(false && "ELECTION message should not be nominating us");
            }

            add_to_cache(msg, el_state);
            propagate(msg, el_state);
            lg->debug("Received election message from " + hb->get_member_by_id(msg.common.id).hostname +
                " with initiator ID " + std::to_string(msg.initiator_id) +
                " and vote ID " + std::to_string(msg.vote_id) +
                " while in state ELECTED");
            transition(elected, electing);
            break;
        }
        case normal: assert(false && "Should not be in state NORMAL while receiving election message");
        case election_init: assert(false && "Should not be in state ELECTION_INIT while receiving election message");
        default:
            break;
    }
}

void election_impl::process_elected_msg(unlocked<election_state> const& el_state, election_message::elected const& msg) {
    switch (el_state->state) {
        case electing: {
            hb->run_atomically_with_mem_list([&] () mutable {
                // If the new master has failed during the election
                if (hb->get_member_by_id(msg.master_id).id != msg.master_id) {
                    lg->info("New master failed during the election, restarting election");

                    // Restart the election by going back to election_wait
                    // We choose election_wait instead of election_init since some nodes may have
                    //  accepted this node as the master node and reallowed nodes to join
                    el_state->highest_initiator_id = 0;
                    transition(electing, election_wait);
                } else {
                    el_state->master_node = hb->get_member_by_id(msg.master_id);
                    lg->info("Election succeeded in electing node at " + el_state->master_node.hostname + " with id " +
                        std::to_string(el_state->master_node.id) + " as master node");

                    // Pass the message along to the next node
                    auto propagated_msg = election_message::elected(hb->get_id(), msg.common.uuid, el_state->master_node.id);
                    enqueue_message(hb->get_successor().hostname, propagated_msg, el_state);
                    transition(electing, normal);
                }
            });
            break;
        }
        case elected: {
            if (msg.master_id == hb->get_id()) {
                lg->info("We have been elected as the master node");
                el_state->master_node = hb->get_member_by_id(msg.master_id);
                transition(elected, normal);
            } else {
                assert(false && "Severe unrecoverable error has occurred that will lead to split braining");
            }
            break;
        }
        case normal: assert(false && "Should not be in state NORMAL while receiving elected message");
        case election_wait: assert(false && "Should not be in state ELECTION_WAIT while receiving elected message");
        case election_init: assert(false && "Should not be in state ELECTION_INIT while receiving elected message");
        case no_master: assert(false && "Should not be in state NO_MASTER while receiving elected message");
        default:
            break;
    }
}

void election_impl::process_proposal_msg(unlocked<election_state> const& el_state, election_message::proposal const& msg) {
    lg->info("Received proposal to restart election from " + hb->get_member_by_id(msg.common.id).hostname +
        ", passing on proposal message and restarting election");

    // Discard all election state
    el_state->highest_initiator_id = 0;
    el_state->message_queue.clear();

    // And propagate the proposal message around the circle
    auto new_msg = election_message::proposal(hb->get_id(), msg.common.uuid);
    enqueue_message(hb->get_successor().hostname, new_msg, el_state);

    // Wait for an election to begin
    transition(el_state->state, election_wait);
}

void election_impl::server_thread_function() {
    server->start_server(config->get_election_port());

    int size;
    char buf[1024];

    while (running.load()) {
        // Listen for messages and process each one
        if ((size = server->recv(buf, 1024)) <= 0) {
            continue;
        }

        unlocked<election_state> el_state = el_state_lock();

        // TODO: replace manual checking of message type with messaging
        election_message::common_data common;
        // The offset of 4 comes from our abuse of our knowledge of how serializable lays out the data
        // This is really bad and should be removed!
        if (!common.deserialize_from(string(buf + 4, size - 4), true)) {
            lg->debug("Received malformed election message! Missing all data");
            continue;
        }

        switch (common.type) {
            case static_cast<uint32_t>(election_message::msg_type::introduction): {
                if (auto msg = filter_msg<election_message::introduction>(el_state, buf, size)) {
                    process_introduction_msg(el_state, msg.value());
                }
                break;
            }
            case static_cast<uint32_t>(election_message::msg_type::election): {
                if (auto msg = filter_msg<election_message::election>(el_state, buf, size)) {
                    process_election_msg(el_state, msg.value());
                }
                break;
            }
            case static_cast<uint32_t>(election_message::msg_type::elected): {
                if (auto msg = filter_msg<election_message::elected>(el_state, buf, size)) {
                    process_elected_msg(el_state, msg.value());
                }
                break;
            }
            case static_cast<uint32_t>(election_message::msg_type::proposal): {
                if (auto msg = filter_msg<election_message::proposal>(el_state, buf, size)) {
                    process_proposal_msg(el_state, msg.value());
                }
                break;
            }
            default: lg->debug("Received malformed election message! Invalid message type");
        }
    }
}

void election_impl::client_thread_function() {
    while (running.load()) {
        { // Atomic block to access message_queue
            unlocked<election_state> el_state = el_state_lock();

            // Pop off the messages that are waiting to be sent
            for (auto const& [dest, msg] : el_state->message_queue.pop()) {
                client->send(dest, config->get_election_port(), msg);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(message_interval_ms));
    }
}

// Calls the callback, providing the master node and a boolean indicating whether or not
// there currently is a master node. If the boolean is false, garbage data is given as the master node.
// The callback will run atomically with all other election logic
void election_impl::get_master_node(std::function<void(member const&, bool)> callback) const {
    unlocked<election_state> el_state = el_state_lock();
    callback(el_state->master_node, el_state->state == normal);
}

void election_impl::wait_master_node(std::function<void(member const&)> callback) const {
    while (true) {
        if (!running.load()) {
            callback(member());
            return;
        }
        bool done = false;
        get_master_node([&] (member const& master, bool succeeded) {
            if (succeeded) {
                callback(master);
                done = true;
            }
        });
        if (true) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

register_auto<election, election_impl> register_election;
