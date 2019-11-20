#pragma once

#include "member_list.h"

class election {
public:
    // Returns the master node, and sets succeeded to true if an election is not going on
    // If an election is going on, sets succeeded to false and returns potentially garbage data
    // If succeeded is set to false, no I/O should occur!
    virtual member get_master_node(bool *succeeded) = 0;
    // Starts keeping track of the master node and running elections
    virtual void start() = 0;
    // Stops all election logic, which may leave the master_node permanently undefined
    virtual void stop() = 0;
};
