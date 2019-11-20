#pragma once

#include "logging.h"
#include "environment.h"
#include "configuration.h"

#include <string>
#include <vector>
#include <list>
#include <chrono>
#include <ctime>
#include <memory>

using namespace std::chrono;

typedef struct member {
    member() : id(0), hostname("") {}
    uint32_t id;
    std::string hostname;
    uint64_t last_heartbeat;
    bool operator==(const member &m);
} member;

// A linked list of members sorted by ID
class member_list {
public:
    // Initialize the member list with the local hostname
    member_list(environment &env) :
        local_hostname(env.get<configuration>()->get_hostname()),
        lg(env.get<logger_factory>()->get_logger("member_list")) {}

    ~member_list() {
        node *next;
        for (node *cur = head; cur != nullptr; cur = next) {
            next = cur->next;
            delete cur;
        }
    }

    // Adds a member to the membership list using hostname and ID and returns the ID
    uint32_t add_member(std::string hostname, uint32_t id);
    // Gets a member from the membership list by ID
    member get_member_by_id(uint32_t id);
    // Removes a member from the membership list
    void remove_member(uint32_t id);
    // Updates the heartbeat for a member to the current time
    void update_heartbeat(uint32_t id);
    // Gets a list of the 2 successors and 2 predecessors (or fewer if there are <5 members)
    std::vector<member> get_neighbors();
    // Get the number of members total
    uint32_t num_members();
    // Whether or not we are in the member list
    bool joined_list();
    // Gets a list of all the members (to be used by introducer)
    std::vector<member> get_members();
    // Gets the successor to the node with the given ID
    // Returns a member with ID 0 if the given ID was not found
    member get_successor(uint32_t id);
private:
    typedef struct node {
        member m;
        node *next;
    } node;

    node *head = nullptr;

    std::string local_hostname;
    std::unique_ptr<logger> lg;
};
