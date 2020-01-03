#pragma once

#include "member_list.h"

#include <functional>

class election {
public:
    // Calls the callback, providing the master node and a boolean indicating whether or not
    // there currently is a master node. If the boolean is false, garbage data is given as the master node.
    // The callback will run atomically with all other election logic
    virtual void get_master_node(std::function<void(member const&, bool)> callback) const = 0;
    // Waits for the master node to be elected and calls the provided callback. Returns an empty member if not running
    virtual void wait_master_node(std::function<void(member const&)> callback) const = 0;
    // Starts keeping track of the master node and running elections
    virtual void start() = 0;
    // Stops all election logic, which may leave the master_node permanently undefined
    virtual void stop() = 0;
};
