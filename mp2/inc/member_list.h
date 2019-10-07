#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <list>

#include <chrono>
#include <ctime>

using namespace std::chrono;

typedef struct member {
    uint32_t id;
    std::string hostname;
    uint64_t last_heartbeat;
    bool operator==(const member &m);
} member;

// A linked list of members sorted by ID
class member_list {
public:
    // Initialize the member list with the local hostname
    member_list(std::string local_hostname_) :
        local_hostname(local_hostname_) {}

    member_list(const member_list &m) {
        local_hostname = m.local_hostname;
        list = m.list;
    }

    // Adds ourselves as the first introducer and returns the ID
    uint32_t add_self_as_introducer(std::string hostname, int join_time);
    // Adds a member to the membership list using hostname and ID and returns the ID
    uint32_t add_member(std::string hostname, uint32_t id);
    // Removes a member from the membership list
    member get_member_by_id(uint32_t id);
    // Removes a member from the membership list
    void remove_member(uint32_t id);
    // Updates the heartbeat for a member to the current time
    void update_heartbeat(uint32_t id);
    // Gets a list of the 2 successors and 2 predecessors (or fewer if there are <5 members)
    std::vector<member> get_neighbors();
    // Get the number of members total
    uint32_t num_members();
    // Gets the list of members (for testing)
    std::list<member> __get_internal_list();
private:
    std::string local_hostname = "";
    std::list<member> list;
    std::list<member>::iterator self;
};
