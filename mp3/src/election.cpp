#include "election.h"
#include "election.hpp"
#include "environment.h"

#include <functional>
#include <chrono>
#include <cassert>
#include <random>
#include <string>
#include <algorithm>

using std::unique_ptr;

election_impl::election_impl(environment &env)
    : hb(env.get<heartbeater>()),
      lg(env.get<logger_factory>()->get_logger("election")),
      client(env.get<udp_factory>()->get_udp_client()),
      server(env.get<udp_factory>()->get_udp_server()),
      config(env.get<configuration>()), running(false)
{
    if (config->is_first_node()) {
        // Set ourselves as the master node
        master_node = hb->get_member_by_id(hb->get_id());
        state = normal;
    } else {
        master_node.id = 0;
        state = no_master;
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
    timer_on = false;

    // Initialize RNG with a slight delay so that unit tests have different seeds
    mt_rand = std::mt19937(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

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
                lg->info("Master node failed! Beginning election process");
                highest_initiator_id = 0;
                transition(normal, election_wait);
            }
        }
    };

    hb->on_fail(callback);
    hb->on_leave(callback);

    // If we are the master node, tell new nodes that we are the master node
    std::function<void(member)> join_callback = [this](member m) {
        { // Atomic block for accessing the state and the master node
            std::lock_guard<std::recursive_mutex> guard(state_mutex);

            if (state == normal && master_node.id == hb->get_id()) {
                lg->debug("Telling node at " + m.hostname + " that we are the master node");

                // Tell the new node we are the master node
                election_message intro_msg(hb->get_id(), mt_rand());
                intro_msg.set_type_introduction(master_node.id);
                enqueue_message(m.hostname, intro_msg, introduction_message_redundancy);
            }

            // If the master node is down, the new node will in state NO_MASTER, and will
            // simply participate in the upcoming elections as a normal node
        }
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
std::string election_impl::print_state(election_state s) {
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
void election_impl::transition(election_state origin_state, election_state dest_state) {
    std::lock_guard<std::recursive_mutex> guard(state_mutex);

    if (state != origin_state) {
        assert(false && "Incorrect origin_state for transition");
    }

    // Cancel the timer if it was already running
    stop_timer();

    state = dest_state;
    lg->debug("Election state transition " + print_state(origin_state) + " -> " + print_state(state));

    switch (state) {
        case election_wait: {
            hb->lock_new_joins();

            // Start timer with a range so that not all nodes send initiation messages
            start_timer(MEMBER_LIST_STABILIZATION_TIME + (std::rand() % MEMBER_LIST_STABILIZATION_RANGE));
            lg->debug("Waiting for membership list to stabilize or for an election to be initiated");
            break;
        }
        case election_init: {
            // Send an election initiation message to our successor
            election_message init_msg(hb->get_id(), mt_rand());
            init_msg.set_type_election(hb->get_id(), hb->get_id());
            enqueue_message(hb->get_successor().hostname, init_msg, message_redundancy);

            highest_initiator_id = hb->get_id();
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
            election_message elected_msg(hb->get_id(), mt_rand());
            elected_msg.set_type_elected(hb->get_id());
            enqueue_message(hb->get_successor().hostname, elected_msg, message_redundancy);
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
    std::lock_guard<std::recursive_mutex> guard(timer_mutex);

    timer_on = true;
    timer = time;
    prev_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Stops the timer
void election_impl::stop_timer() {
    std::lock_guard<std::recursive_mutex> guard(timer_mutex);

    timer_on = false;
    timer = 0;
}

// Updates the timer and performs the appropriate action in the state machine if the timer reaches 0
void election_impl::update_timer() {
    uint64_t elapsed_time = 0;
    for (; running.load(); std::this_thread::sleep_for(std::chrono::milliseconds(100))) {
        std::lock_guard<std::recursive_mutex> guard_state(state_mutex);
        std::lock_guard<std::recursive_mutex> guard_timer(timer_mutex);

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

        switch (state) {
            case election_wait: {
                lg->debug("Membership list has stabilized, sending election initiation message");
                transition(election_wait, election_init);
                break;
            }
            case electing:
            case elected: {
                highest_initiator_id = 0;
                lg->info("Election has timed out, restarting election by sending PROPOSAL message");

                // Create a proposal message to send to the next node to start a new election
                election_message proposal_msg(hb->get_id(), mt_rand());
                proposal_msg.set_type_proposal();
                enqueue_message(hb->get_successor().hostname, proposal_msg, message_redundancy);

                // Transition to a waiting state while the proposal message makes the rounds and
                //  the membership list stabilizes
                transition(state, election_wait);
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
            seen_message_ids.pop();
        }
        elapsed_time = new_time;
    }
}

// Keeps track of the highest initiator ID seen for ELECTION messages so far
void election_impl::add_to_cache(election_message msg) {
    assert(msg.get_type() == election_message::msg_type::election);

    if (highest_initiator_id < msg.get_initiator_id()) {
        highest_initiator_id = msg.get_initiator_id();
    }
}

// Passes on an ELECTION message as defined by the protocol
void election_impl::propagate(election_message msg) {
    assert(msg.get_type() == election_message::msg_type::election);

    if (msg.get_initiator_id() >= highest_initiator_id) {
        assert(msg.get_vote_id() != hb->get_id());

        // Create the new message to pass on
        // If we are the initiator, change the UUID of the message to indicate that
        //  we are beginning the second cycle through the ring
        uint32_t uuid = (msg.get_initiator_id() == hb->get_id()) ? mt_rand() : msg.get_uuid();
        election_message new_msg(hb->get_id(), uuid);
        if (msg.get_vote_id() > hb->get_id()) {
            new_msg.set_type_election(msg.get_initiator_id(), msg.get_vote_id());
        } else {
            new_msg.set_type_election(msg.get_initiator_id(), hb->get_id());
        }

        // Send the message
        enqueue_message(hb->get_successor().hostname, new_msg, message_redundancy);
    }
}

// Enqueues a message into the message_queue and prints debug information
void election_impl::enqueue_message(std::string dest, election_message msg, int redundancy) {
    // Print log information first
    if (msg.get_type() == election_message::msg_type::introduction) {
        lg->debug("Informing new node at " + dest + " that master node is " +
            hb->get_member_by_id(msg.get_master_id()).hostname);
    }

    if (msg.get_type() == election_message::msg_type::election) {
        lg->debug("Sending ELECTION message with initiator ID " + std::to_string(msg.get_initiator_id()) +
            " and vote ID " + std::to_string(msg.get_vote_id()) + " to node at " + dest);
    }

    if (msg.get_type() == election_message::msg_type::elected) {
        lg->debug("Sending ELECTED message with master ID " + std::to_string(msg.get_master_id()) +
            " to node at " + dest);
    }

    if (msg.get_type() == election_message::msg_type::proposal) {
        lg->debug("Sending PROPOSAL message with UUID " + std::to_string(msg.get_uuid()) +
            " to node at " + dest);
    }

    if (msg.get_type() == election_message::msg_type::empty) {
        assert(false && "Should never send an empty message");
    }

    // Actually enqueue the message
    message_queue.push(std::make_tuple(dest, msg), redundancy);
}

void election_impl::server_thread_function() {
    server->start_server(config->get_election_port());

    int size;
    char buf[1024];

    while (running.load()) {
        // Listen for messages and process each one
        if ((size = server->recv(buf, 1024)) > 0) {
            std::lock_guard<std::recursive_mutex> guard(state_mutex);

            election_message msg(buf, size);

            if (!msg.is_well_formed()) {
                lg->debug("Received malformed election message!");
                continue;
            }

            if (msg.get_type() == election_message::msg_type::empty) {
                assert(false && "Message should not be empty at this point");
            }

            // Make sure that the message originates from within our group
            if (hb->get_member_by_id(msg.get_id()).id != msg.get_id()) {
                lg->trace("Ignoring election message from " + std::to_string(msg.get_id()) + " because they are not in the group");
                continue;
            }

            // Check that we have not seen a message with this ID yet
            std::vector<uint32_t> seen_ids = seen_message_ids.peek();
            if (std::find(seen_ids.begin(), seen_ids.end(), msg.get_uuid()) != seen_ids.end()) {
                // Ignore the message
                lg->trace("Received duplicate message of type " + msg.print_type() + " from host at " +
                    hb->get_member_by_id(msg.get_id()).hostname);
                continue;
            }
            // Mark this message ID as seen
            seen_message_ids.push(msg.get_uuid(), seen_ids_ttl);

            if (msg.get_type() == election_message::msg_type::election) {
                switch (state) {
                    case no_master: // If we joined after the master node failed, just participate in the elections as normal
                    case election_wait: {
                        add_to_cache(msg);
                        propagate(msg);
                        lg->debug("Received election message from " + hb->get_member_by_id(msg.get_id()).hostname +
                            " with initiator ID " + std::to_string(msg.get_initiator_id()) +
                            " and vote ID " + std::to_string(msg.get_vote_id()));
                        transition(election_wait, electing);
                        break;
                    }
                    case electing: {
                        if (msg.get_vote_id() == hb->get_id()) {
                            lg->debug("Received election message with our ID from " + hb->get_member_by_id(msg.get_id()).hostname);

                            transition(electing, elected);
                        } else {
                            add_to_cache(msg);
                            propagate(msg);
                            lg->debug("Received election message from " + hb->get_member_by_id(msg.get_id()).hostname +
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
                        lg->debug("Received election message from " + hb->get_member_by_id(msg.get_id()).hostname +
                            " with initiator ID " + std::to_string(msg.get_initiator_id()) +
                            " and vote ID " + std::to_string(msg.get_vote_id()) +
                            " while in state ELECTED");
                        transition(elected, electing);
                        break;
                    }
                    case normal: assert(false && "Should not be in state NORMAL while receiving election message");
                    case election_init: assert(false && "Locking of state_mutex is incorrect somewhere");
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
                                lg->info("New master failed during the election, restarting election");

                                // Restart the election by going back to election_wait
                                // We choose election_wait instead of election_init since some nodes may have
                                //  accepted this node as the master node and reallowed nodes to join
                                highest_initiator_id = 0;
                                transition(electing, election_wait);
                            } else {
                                master_node = hb->get_member_by_id(msg.get_master_id());
                                lg->info("Election succeeded in electing node at " + master_node.hostname + " with id " +
                                    std::to_string(master_node.id) + " as master node");

                                // Pass the message along to the next node
                                election_message propagated_msg(hb->get_id(), msg.get_uuid());
                                propagated_msg.set_type_elected(master_node.id);
                                enqueue_message(hb->get_successor().hostname, propagated_msg, message_redundancy);
                                transition(electing, normal);
                            }
                        });
                        break;
                    }
                    case elected: {
                        if (msg.get_master_id() == hb->get_id()) {
                            lg->info("We have been elected as the master node");
                            master_node = hb->get_member_by_id(msg.get_master_id());
                            transition(elected, normal);
                        } else {
                            assert(false && "Severe unrecoverable error has occurred that will lead to split braining");
                        }
                        break;
                    }
                    case normal: assert(false && "Should not be in state NORMAL while receiving elected message");
                    case election_wait: assert(false && "Should not be in state ELECTION_WAIT while receiving elected message");
                    case election_init: assert(false && "Locking of state_mutex is incorrect somewhere");
                    case no_master: assert(false && "Should not be in state NO_MASTER while receiving elected message");
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

                            lg->info("Received master node at " + master_node.hostname + " with id " +
                                std::to_string(master_node.id));

                            transition(no_master, normal);
                        }
                    }
                });
            }

            if (msg.get_type() == election_message::msg_type::proposal) {
                lg->info("Received proposal to restart election from " + hb->get_member_by_id(msg.get_id()).hostname +
                    ", passing on proposal message and restarting election");

                // Discard all election state
                highest_initiator_id = 0;
                message_queue.clear();

                // And propagate the proposal message around the circle
                election_message new_msg(hb->get_id(), msg.get_uuid());
                new_msg.set_type_proposal();
                enqueue_message(hb->get_successor().hostname, new_msg, message_redundancy);

                // Wait for an election to begin
                transition(state, election_wait);
            }
        }
    }
}

void election_impl::client_thread_function() {
    while (running.load()) {
        { // Atomic block to access message_queue
            std::lock_guard<std::recursive_mutex> guard(state_mutex);

            // Pop off the messages that are waiting to be sent
            std::vector<std::tuple<std::string, election_message>> messages = message_queue.pop();

            for (auto m : messages) {
                std::string dest = std::get<0>(m);
                election_message msg = std::get<1>(m);

                std::string msg_str = msg.serialize();

                client->send(dest, config->get_election_port(), msg_str);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(message_interval_ms));
    }
}

// Calls the callback, providing the master node and a boolean indicating whether or not
// there currently is a master node. If the boolean is false, garbage data is given as the master node.
// The callback will run atomically with all other election logic
void election_impl::get_master_node(std::function<void(member, bool)> callback) {
    std::lock_guard<std::recursive_mutex> guard(state_mutex);
    callback(master_node, state == normal);
}

void election_impl::wait_master_node(std::function<void(member)> callback) {
    bool done = false;
    while (!done) {
        get_master_node([&done, &callback] (member master, bool succeeded) {
            if (succeeded) {
                callback(master);
                done = true;
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

register_auto<election, election_impl> register_election;
