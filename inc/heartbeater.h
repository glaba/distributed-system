#pragma once

#include "member_list.h"

#include <vector>
#include <functional>
#include <string>

// The amount of time it takes for the loss or join of any node to be reflected in all nodes
#define HEARTBEATER_STABILIZATION_TIME 6000

class heartbeater {
public:
    virtual ~heartbeater() {}
    // Starts the heartbeater
    virtual void start() = 0;
    // Stops the heartbeater synchronously
    virtual void stop() = 0;
    // Returns the list of members of the group that this node is aware of
    virtual auto get_members() const -> std::vector<member> = 0;
    // Gets the member object corresponding to the provided ID
    virtual auto get_member_by_id(uint32_t id) const -> member = 0;
    // Returns the next node after this one in the membership list
    // If we have not yet joined the group, returns an empty member
    virtual auto get_successor() const -> member = 0;
    // Runs the provided function atomically with any functions that read or write to the membership list
    virtual void run_atomically_with_mem_list(std::function<void()> const& fn) const = 0;
    // Initiates an async request to join the group by sending a message to a node in the group
    virtual void join_group(std::string const& node) = 0;
    // Sends a message to peers stating that we are leaving
    virtual void leave_group() = 0;
    // Returns the ID of the current node, which will be 0 if we have not joined the group
    virtual auto get_id() const -> uint32_t = 0;
    // Prevents any nodes from joining
    virtual void lock_new_joins() = 0;
    // Allows nodes to join again
    virtual void unlock_new_joins() = 0;

    // Adds to a list of handlers that will be called when membership list changes
    // The membership list will not be modified while the handlers are running
    virtual void on_fail(std::function<void(member const&)>) = 0;
    virtual void on_leave(std::function<void(member const&)>) = 0;
    virtual void on_join(std::function<void(member const&)>) = 0;
};
