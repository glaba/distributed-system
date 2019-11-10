#include "election.h"

#include <functional>
#include <chrono>
#include <cassert>
#include <random>
#include <string>
#include <algorithm>

using std::unique_ptr;

election::election(heartbeater_intf *hb_, logger *lg_, udp_client_intf *client_, udp_server_intf *server_, uint16_t port_)
        : hb(hb_), lg(lg_), client(client_), server(server_), port(port_) {

    if (hb->is_introducer()) {
        // Set ourselves as the master node
        master_node = hb->get_member_by_id(hb->get_id());
        state = normal;
    } else {
        master_node.id = 0;
        state = no_master;
    }
}

// Starts keeping track of the master node and running elections
void election::start() {
    lg->log("Starting election");

    running = true;
    timer_on = false;

    timer_thread = std::make_unique<std::thread>([this] {update_timer();});
    timer_thread->detach();

    server_thread = std::make_unique<std::thread>([this] {server_thread_function();});
    server_thread->detach();

    client_thread = std::make_unique<std::thread>([this] {client_thread_function();});
    client_thread->detach();

    std::function<void(member)> callback = [this](member m) {
        { // Atomic block for accessing the state
            std::lock_guard<std::recursive_mutex> guard(state_mutex);

            // If we don't know who the master node is, we cannot participate in elections
            if (state == no_master) {
                assert(false && "Did not receive master node introduction message at all OR in time for election");
            }

            if (state != normal) {
                return;
            }

            if (m.id == master_node.id) {
                lg->log("Master node failed! Beginning election process");
                highest_initiator_id = 0;
                transition(normal, election_wait);
            }
        }
    };

    hb->on_fail(callback);
    hb->on_leave(callback);

    // If we are the introducer, add a callback to tell new nodes who the master node is
    if (hb->is_introducer()) {
        std::function<void(member)> join_callback = [this](member m) {
            { // Atomic block for accessing the state and the master node
                std::lock_guard<std::recursive_mutex> guard(state_mutex);

                // A new node should not have been allowed to join if state != normal
                assert(state == normal);

                // Send the master node to the new node
                election_message intro_msg(hb->get_id());
                intro_msg.set_type_introduction(master_node.id);
                message_queue.push(std::make_tuple(m.hostname, intro_msg), message_redundancy);
            }
        };

        hb->on_join(join_callback);
    }
}

// Stops all threads and election logic (may leave master_node in an invalid state)
void election::stop() {
    lg->log("Stopping election");

    server->stop_server();
    running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

// Debugging function to print a string value for the enum
std::string election::print_state(election_state s) {
    switch (s) {
        case normal: return "NORMAL";
        case election_wait: return "ELECTION_WAIT";
        case election_init: return "ELECTION_INIT";
        case electing: return "ELECTING";
        case elected: return "ELECTED";
        default: return "";
    }
}

// Transitions the current state to the given state
void election::transition(election_state origin_state, election_state dest_state) {
    std::lock_guard<std::recursive_mutex> guard(state_mutex);

    if (state != origin_state) {
        assert(false && "Incorrect origin_state for transition");
    }

    state = dest_state;
    lg->log("Election state transition " + print_state(origin_state) + " -> " + print_state(state));

    switch (state) {
        case election_wait: {
            hb->lock_new_joins();

            // Start timer with a range so that not all nodes send initiation messages
            start_timer(MEMBER_LIST_STABILIZATION_TIME + (std::rand() % MEMBER_LIST_STABILIZATION_RANGE));
            lg->log("Waiting for membership list to stabilize or for an election to be initiated");
            break;
        }
        case election_init: {
            // Send an election initiation message to our successor
            election_message init_msg(hb->get_id());
            init_msg.set_type_election(hb->get_id(), hb->get_id());
            message_queue.push(std::make_tuple(hb->get_successor().hostname, init_msg), 1);

            highest_initiator_id = hb->get_id();
            transition(election_init, electing);
            break;
        }
        case electing: {
            // Set timer at which point the election times out and we restart the election
            start_timer(ELECTION_TIMEOUT_TIME);
            lg->log("Election has begun, timer waiting for election to time out");
            break;
        }
        case elected: {
            // Send a message to the successor stating that we have been elected
            election_message elected_msg(hb->get_id());
            elected_msg.set_type_elected(hb->get_id());
            message_queue.push(std::make_tuple(hb->get_successor().hostname, elected_msg), 1);
            lg->log("We have been nominated as the new master node");

            // Set a timer in case the ELECTED message gets lost in the ring
            start_timer(ELECTED_TIMEOUT_TIME);
            lg->log("Timer set to wait for election to time out");
            break;
        }
        case normal: {
            // Once we transition back into normal state, new nodes can join again
            hb->unlock_new_joins();
            break;
        }
        default:
            break;
    }
}

// Starts the timer to run for the given number of milliseconds
void election::start_timer(uint32_t time) {
    std::lock_guard<std::mutex> guard(timer_mutex);

    timer_on = true;
    timer = time;
    prev_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Updates the timer and performs the appropriate action in the state machine if the timer reaches 0
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
            std::lock_guard<std::recursive_mutex> guard(state_mutex);

            switch (state) {
                case election_wait: {
                    lg->log("Membership list has stabilized, sending election initiation message");
                    transition(election_wait, election_init);
                    break;
                }
                case electing: {
                    highest_initiator_id = 0;
                    lg->log("Election has timed out, restarting election");
                    transition(electing, election_wait);
                    break;
                }
                case elected: {
                    highest_initiator_id = 0;
                    lg->log("Elected message failed to go around ring, restarting election");
                    transition(elected, election_wait);
                    break;
                }
                case normal:
                case election_init:
                default:
                    break;
            }
        }
    }
}

// Keeps track of the highest initiator ID seen for ELECTION messages so far
void election::add_to_cache(election_message msg) {
    assert(msg.get_type() == election_message::msg_type::election);

    if (highest_initiator_id < msg.get_initiator_id()) {
        highest_initiator_id = msg.get_initiator_id();
    }
}

// Passes on an ELECTION message as defined by the protocol
void election::propagate(election_message msg) {
    assert(msg.get_type() == election_message::msg_type::election);

    if (msg.get_initiator_id() >= highest_initiator_id) {
        assert(msg.get_vote_id() != hb->get_id());

        // Create the new message to pass on
        election_message new_msg(hb->get_id());
        if (msg.get_vote_id() > hb->get_id()) {
            new_msg.set_type_election(msg.get_initiator_id(), msg.get_vote_id());
        } else {
            new_msg.set_type_election(msg.get_initiator_id(), hb->get_id());
        }

        // Send the message
        message_queue.push(std::make_tuple(hb->get_successor().hostname, new_msg), 1);
    }
}

void election::server_thread_function() {
    server->start_server(port);

    int size;
    char buf[1024];

    while (running.load()) {
        // Listen for messages and process each one
        if ((size = server->recv(buf, 1024)) > 0) {
            std::lock_guard<std::recursive_mutex> guard(state_mutex);

            election_message msg(buf, size);

            if (!msg.is_well_formed()) {
                lg->log("Received malformed election message! Reason: " + msg.why_malformed());
                continue;
            }

            if (msg.get_type() == election_message::msg_type::empty) {
                assert(false && "Message should not be empty at this point");
            }

            // Make sure that the message originates from within our group
            if (hb->get_member_by_id(msg.get_id()).id != msg.get_id()) {
                lg->log("Ignoring election message from " + std::to_string(msg.get_id()) + " because they are not in the group");
                continue;
            }

            if (msg.get_type() == election_message::msg_type::election) {
                switch (state) {
                    case election_wait: {
                        add_to_cache(msg);
                        propagate(msg);
                        lg->log("Received election message from " + hb->get_member_by_id(msg.get_id()).hostname +
                            " with initiator ID " + std::to_string(msg.get_initiator_id()) +
                            " and vote ID " + std::to_string(msg.get_vote_id()));
                        transition(election_wait, electing);
                        break;
                    }
                    case electing: {
                        if (msg.get_vote_id() == hb->get_id()) {
                            lg->log("Received election message with our ID from " + hb->get_member_by_id(msg.get_id()).hostname);

                            transition(electing, elected);
                        } else {
                            add_to_cache(msg);
                            propagate(msg);
                            lg->log("Received election message from " + hb->get_member_by_id(msg.get_id()).hostname +
                                " with initiator ID " + std::to_string(msg.get_initiator_id()) +
                                " and vote ID " + std::to_string(msg.get_vote_id()));
                        }
                        break;
                    }
                    case elected: {
                        // Make sure the ELECTION message is nominating a node other than us
                        if (msg.get_id() == hb->get_id()) {
                            assert(false && "ELECTION message should not be nominating us");
                        }

                        add_to_cache(msg);
                        propagate(msg);
                        lg->log("Received election message from " + hb->get_member_by_id(msg.get_id()).hostname +
                            " with initiator ID " + std::to_string(msg.get_initiator_id()) +
                            " and vote ID " + std::to_string(msg.get_vote_id()) +
                            " while in state ELECTED");
                        transition(elected, electing);
                        break;
                    }
                    case normal:
                    case election_init:
                    default:
                        break;
                }
            }

            if (msg.get_type() == election_message::msg_type::elected) {
                switch (state) {
                    case electing: {
                        hb->run_atomically_with_mem_list([this, msg] () mutable {
                            // If the new master has failed during the election
                            if (hb->get_member_by_id(msg.get_master_id()).id != msg.get_master_id()) {
                                lg->log("New master failed during the election, restarting election");

                                // Restart the election by going back to election_wait
                                // We choose election_wait instead of election_init since the introducer may have
                                //  accepted this node as the master node and reallowed nodes to join
                                highest_initiator_id = 0;
                                transition(electing, election_wait);
                            } else {
                                master_node = hb->get_member_by_id(msg.get_master_id());
                                lg->log("Election succeeded in electing node at " + master_node.hostname + " with id " +
                                    std::to_string(master_node.id) + " as master node");

                                // Pass the message along to the next node
                                election_message propagated_msg(hb->get_id());
                                propagated_msg.set_type_elected(master_node.id);
                                message_queue.push(std::make_tuple(hb->get_successor().hostname, propagated_msg), 1);
                                transition(electing, normal);
                            }
                        });
                        break;
                    }
                    case elected: {
                        if (msg.get_master_id() == hb->get_id()) {
                            lg->log("We have been elected as the master node");
                            master_node = hb->get_member_by_id(msg.get_master_id());
                            transition(elected, normal);
                        } else {
                            lg->log(std::to_string(msg.get_master_id()) + ", " + std::to_string(hb->get_id()));
                            assert(false && "Severe unrecoverable error has occurred that will lead to split braining");
                        }
                        break;
                    }
                    case normal:
                    case election_wait:
                    case election_init:
                    default:
                        break;
                }
            }

            if (msg.get_type() == election_message::msg_type::introduction) {
                hb->run_atomically_with_mem_list([this, msg] () mutable {
                    // Check if we are in the state no_master
                    if (state == no_master) {
                        // Check if the master node is in our membership list
                        if (hb->get_member_by_id(msg.get_master_id()).id == msg.get_master_id()) {
                            master_node = hb->get_member_by_id(msg.get_master_id());

                            lg->log("Received master node at " + master_node.hostname + " with id " +
                                std::to_string(master_node.id) + " from introducer");

                            transition(no_master, normal);
                        }
                    }
                });
            }
        }
    }
}

void election::client_thread_function() {
    while (running.load()) {
        // Pop off the messages that are waiting to be sent
        std::vector<std::tuple<std::string, election_message>> messages = message_queue.pop();

        for (auto m : messages) {
            std::string dest = std::get<0>(m);
            election_message msg = std::get<1>(m);

            // Print log information first
            if (msg.get_type() == election_message::msg_type::introduction) {
                lg->log("Informing new node at " + dest + " that master node is " +
                    hb->get_member_by_id(msg.get_master_id()).hostname);
            }

            if (msg.get_type() == election_message::msg_type::election) {
                lg->log("Sending ELECTION message with initiator ID " + std::to_string(msg.get_initiator_id()) +
                    " and vote ID " + std::to_string(msg.get_vote_id()) + " to node at " + dest);
            }

            if (msg.get_type() == election_message::msg_type::elected) {
                lg->log("Sending ELECTED message with master ID " + std::to_string(msg.get_master_id()) +
                    " to node at " + dest);
            }

            if (msg.get_type() == election_message::msg_type::empty) {
                assert(false && "Should never send an empty message");
            }

            unique_ptr<char[]> buf;
            unsigned length;
            buf = msg.serialize(length);

            client->send(dest, port, buf.get(), length);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(message_interval_ms));
    }
}

// Returns the master node, and sets succeeded to true if an election is not going on
// If an election is going on, sets succeeded to false and returns potentially garbage data
// If succeeded is set to false, no I/O should occur!
member election::get_master_node(bool *succeeded) {
    std::lock_guard<std::recursive_mutex> guard(state_mutex);

    *succeeded = (state == normal);
    return master_node;
}
