#pragma once

#include "member_list.h"
#include "heartbeater.h"
#include "logging.h"
#include "redundant_queue.h"
#include "udp.h"
#include "election_messages.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <memory>

// The minimum amount of time it will take for the member list to stabilize
//  6s in the worst case for the introducer to notice the failure of the master node and prevent new nodes from joining
//  and 6s more for any nodes that joined in the meantime to join all membership lists
#define MEMBER_LIST_STABILIZATION_TIME 12000
// The range we will wait for the member list to stabilize before starting an election is in
// the range (STABILIZATION_TIME, STABILIZATION_TIME + STABILIZATION_RANGE), so that elections
// are not all initiated at roughly the same time
#define MEMBER_LIST_STABILIZATION_RANGE 6000
// The amount of time we will wait before the election times out
#define ELECTION_TIMEOUT_TIME 15000
// The amount of time a node that has been elected will wait before the ELECTED message goes around the ring
#define ELECTED_TIMEOUT_TIME 5000

class election {
public:
    election(heartbeater_intf *hb_, logger *lg_, udp_client_intf *client_, udp_server_intf *server_, uint16_t port_);

    // Returns the master node, and sets succeeded to true if an election is not going on
    // If an election is going on, sets succeeded to false and returns potentially garbage data
    // If succeeded is set to false, no I/O should occur!
    member get_master_node(bool *succeeded);

    // Starts keeping track of the master node and running elections
    void start();

    // Stops all election logic, which may leave the master_node permanently undefined
    void stop();
private:
    // The state of the state machine for the election
    // The state machine essentially works in the following way:
    //  When events occur, two things atomically occur: transition, post-transition action
    //  These 2 things occur atomically thanks to the mutex state_mutex
    enum election_state {
        no_master, normal, election_wait, election_init, electing, elected
    };
    election_state state;
    std::recursive_mutex state_mutex;

    // Transitions the current state to the given state
    void transition(election_state origin_state, election_state dest_state);
    // Variable that indicates that a transition has occurred
    std::atomic<bool> state_changed;

    // Indicates whether or not the election is running
    std::atomic<bool> running;

    // Keeps track of the highest initiator ID seen for ELECTION messages so far
    uint32_t highest_initiator_id;
    void add_to_cache(election_message msg);
    // Passes on an ELECTION message as defined by the protocol
    void propagate(election_message msg);

    // Sets and updates the timer, and when the timer reaches 0, performs the appropriate action depending on the state
    bool timer_on;
    uint32_t timer;
    uint32_t prev_time;
    void start_timer(uint32_t time);
    void update_timer();
    std::unique_ptr<std::thread> timer_thread; // Thread that will update the timer
    std::mutex timer_mutex; // Mutex that protects the timer variables

    // Debugging function to print a string value for the enum
    std::string print_state(election_state s);

    // The heartbeater whose member list we will use to determine the master node
    heartbeater_intf *hb;

    // The logger provided to us
    logger *lg;

    // Number of times to send an introduction message
    // This is higher than the heartbeater redundancy in case the first few heartbeater introducer
    // messages don't arrive, the election introduction message is not ignored
    const int message_redundancy = 8;
    // Time between sending pending messages (in ms)
    const uint64_t message_interval_ms = 250;

    // UDP client and server interfaces for communication with other nodes
    udp_client_intf *client;
    udp_server_intf *server;
    // Thread for the server / client as well as the corresponding functions
    std::unique_ptr<std::thread> server_thread, client_thread;
    void server_thread_function();
    void client_thread_function();
    // Queue of messages to be sent by the client thread -- tuple is of the format (hostname, message)
    redundant_queue<std::tuple<std::string, election_message>> message_queue;

    // The current master node (an ID of 0 means there is no master node)
    member master_node;

    // The port that will be used for elections
    uint16_t port;
};
