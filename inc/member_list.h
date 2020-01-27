#pragma once

#include "logging.h"
#include "environment.h"
#include "configuration.h"
#include "serialization.h"

#include <string>
#include <vector>
#include <list>
#include <chrono>
#include <ctime>
#include <memory>

using namespace std::chrono;

class member : public serializable<member> {
public:
    member() : id(0), hostname("") {}
    member(uint32_t id, std::string hostname) : id(id), hostname(hostname) {}
    auto operator==(const member &m) const -> bool;
    uint32_t id;
    std::string hostname;
    uint64_t last_heartbeat;
};

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
    auto add_member(std::string const& hostname, uint32_t id) -> uint32_t;
    // Gets a member from the membership list by ID
    auto get_member_by_id(uint32_t id) const -> member;
    // Removes a member from the membership list
    void remove_member(uint32_t id);
    // Updates the heartbeat for a member to the current time
    void update_heartbeat(uint32_t id);
    // Get the number of members total
    auto num_members() const -> uint32_t;
    // Gets a list of the 2 successors and 2 predecessors (or fewer if there are <5 members)
    auto get_neighbors() const -> std::vector<member>;
    // Gets a list of all the members
    auto get_members() const -> std::vector<member>;
    // Gets the successor to the node with the given ID
    // Returns a member with ID 0 if the given ID was not found
    auto get_successor(uint32_t id) const -> member;
private:
    // Whether or not we are in the member list
    auto joined_list() const -> bool;

    typedef struct node {
        member m;
        node *next;
    } node;

    node *head = nullptr;

    std::string local_hostname;
    std::unique_ptr<logger> lg;
};
