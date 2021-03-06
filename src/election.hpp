#pragma once

#include "election.h"
#include "member_list.h"
#include "heartbeater.h"
#include "logging.h"
#include "redundant_queue.h"
#include "udp.h"
#include "election_messages.h"
#include "service.h"
#include "environment.h"
#include "locking.h"

#include <atomic>
#include <thread>
#include <memory>
#include <random>
#include <unordered_set>

// The minimum amount of time it will take for the member list to stabilize
//  6s in the worst case for all nodes to notice the failure of the master node and prevent new nodes from joining
//  and 6s more for any nodes that joined in the meantime to join all membership lists
#define MEMBER_LIST_STABILIZATION_TIME (2 * HEARTBEATER_STABILIZATION_TIME)
// The range we will wait for the member list to stabilize before starting an election is in
// the range (STABILIZATION_TIME, STABILIZATION_TIME + STABILIZATION_RANGE), so that elections
// are not all initiated at roughly the same time
#define MEMBER_LIST_STABILIZATION_RANGE HEARTBEATER_STABILIZATION_TIME
// The amount of time we will wait before the election times out
#define ELECTION_TIMEOUT_TIME 15000
// The amount of time a node that has been elected will wait before the ELECTED message goes around the ring
#define ELECTED_TIMEOUT_TIME 5000

class election_impl : public election, public service_impl<election_impl> {
public:
    election_impl(environment &env);

    void get_master_node(std::function<void(member const&, bool)> callback) const;
    void wait_master_node(std::function<void(member const&)> callback) const;
    void start();
    void stop();
private:
    // The state of the state machine for the election
    // When events occur, two things atomically occur: transition, post-transition action
    enum election_state_enum {
        no_master, normal, election_wait, election_init, electing, elected
    };
    struct election_state {
        election_state_enum state;
        // Queue of messages to be sent by the client thread -- tuple is of the format (hostname, message)
        redundant_queue<std::tuple<std::string, election_message>> message_queue;
        // A "redundant" queue that will be popped every 1 minute containing all the message IDs seen so far
        redundant_queue<uint32_t> seen_message_ids;
        // The random number generator which we will use to generate message IDs
        std::mt19937 mt_rand;
        // Keeps track of the highest initiator ID seen for ELECTION messages so far
        uint32_t highest_initiator_id;
        // The current master node (an ID of 0 means there is no master node)
        member master_node;
    };
    locked<election_state> el_state_lock;

    // Transitions the current state to the given state
    void transition(election_state_enum origin_state, election_state_enum dest_state);

    void add_to_cache(election_message const& msg, unlocked<election_state> const& el_state);
    // Passes on an ELECTION message as defined by the protocol
    void propagate(election_message const& msg, unlocked<election_state> const& el_state);

    // Sets and updates the timer, and when the timer reaches 0, performs the appropriate action depending on the state
    struct timer_state {
        bool timer_on;
        uint32_t timer;
        uint32_t prev_time;
    };
    void start_timer(uint32_t time);
    void stop_timer();
    void update_timer();
    locked<timer_state> tm_state_lock;

    // Debugging function to print a string value for the enum
    auto print_state(election_state_enum const& s) -> std::string;

    // Services that we depend on
    heartbeater *hb;
    std::unique_ptr<logger> lg;
    std::unique_ptr<udp_client> client;
    std::unique_ptr<udp_server> server;
    configuration *config;

    // Number of times to send an introduction message
    // This is higher than the heartbeater redundancy in case the first few heartbeater introducer
    // messages don't arrive, the election introduction message is not ignored
    const int introduction_message_redundancy = 8;
    // Number of times to send all other message types
    const int message_redundancy = 3;
    // Time between sending pending messages (in ms)
    const uint64_t message_interval_ms = 250;
    // TTL of the list of seen message IDs in minutes (effectively a limit on how late UDP messages can be delayed)
    const int seen_ids_ttl = 5;

    // Thread functions for the server / client
    void server_thread_function();
    void client_thread_function();
    // Enqueues a message into the message_queue and prints debug information
    void enqueue_message(std::string const& dest, election_message const& msg,
        int redundancy, unlocked<election_state> const& el_state);

    // Indicates whether or not the election is running
    std::atomic<bool> running;
};
