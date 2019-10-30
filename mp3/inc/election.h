#pragma once

#include "member_list.h"
#include "heartbeater.h"
#include "logging.h"

#include <atomic>
#include <mutex>
#include <thread>

// The minimum amount of time it will take for the member list to stabilize
//  6s in the worst case for the introducer to notice the failure of the master node and prevent new nodes from joining
//  and 6s more for any nodes that joined in the meantime to join all membership lists
#define MEMBER_LIST_STABILIZATION_TIME 12000
// The range we will wait for the member list to stabilize before starting an election is in
// the range (STABILIZATION_TIME, STABILIZATION_TIME + STABILIZATION_RANGE), so that elections
// are not all initiated at roughly the same time
#define MEMBER_LIST_STABILIZATION_RANGE 6000
// The amount of time per node we will wait before the election times out
// This is very rough logic, but in the worst case (assuming no failures), it will take 3 cycles
// around the ring for the election to complete, and assuming each message takes 500ms, we get 1500ms per node
#define ELECTION_TIMEOUT_TIME 1500

class election {
public:
    election(heartbeater_intf *hb_, logger *lg_, member master_node_);
    ~election();

    // Returns the master node, and sets succeeded to true if an election is not going on
    // If an election is going on, sets succeeded to false and returns potentially garbage data
    // If succeeded is set to false, no I/O should occur!
    member get_master_node(bool *succeeded);

    // Starts keeping track of the master node and running elections
    void start();

    // Stops all election logic, which may leave the master_node permanently undefined
    void stop() {
        running = false;
    }
private:
    // The state of the state machine for the election
    // The state machine essentially works in the following way:
    //  When events occur, two things atomically occur: transition, post-transition action
    //  These 2 things occur atomically thanks to the mutex state_mutex
    enum election_state {
        normal, election_wait, election_init, electing, elected, failure
    };
    election_state state;
    std::atomic<election_state> next_state; // The next state to transition to
    std::recursive_mutex state_mutex;

    // Transitions the current state to the given state and returns true if the transition succeeded
    // Performs additional checks to make sure that there are no race conditions (which is why origin is needed)
    bool transition(election_state origin_state, election_state dest_state);
    // Variable that indicates that a transition has occurred
    std::atomic<bool> state_changed;

    // A function and associated thread that performs the election logic, and an indicator variable for it to stop
    void run_election();
    std::thread *election_thread;
    std::atomic<bool> running;

    // Sets and updates the timer, and when the timer reaches 0, performs the appropriate action depending on the state
    bool timer_on;
    uint32_t timer;
    uint32_t prev_time;
    void start_timer(uint32_t time);
    void update_timer();
    std::thread *timer_thread; // Thread that will update the timer
    std::mutex timer_mutex; // Mutex that protects the timer variables

    // Debugging function to print a string value for the enum
    std::string print_state(election_state s);

    // The heartbeater whose member list we will use to determine the master node
    heartbeater_intf *hb;

    // The logger provided to us
    logger *lg;

    // The current master node (an ID of 0 means there is no master node)
    member master_node;
};
