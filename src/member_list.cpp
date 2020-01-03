#include "member_list.h"

#include <algorithm>
#include <cassert>

// The amount of time a new member gets before we start to expect heartbeats from it
const uint64_t new_member_heartbeat_slack = 2000;

// Adds a member to the membership list using hostname and ID and returns the ID
auto member_list::add_member(std::string const& hostname, uint32_t id) -> uint32_t {
    uint32_t original_size = num_members();

    node *prev = nullptr;
    node *cur = head;
    // Find the insertion point
    while (cur != nullptr) {
        if (id < cur->m.id)
            break;

        prev = cur;
        cur = cur->next;
    }

    // Create the new node
    node *new_node = new node();
    assert(new_node != nullptr);
    new_node->next = cur;
    new_node->m.id = id;
    new_node->m.hostname = hostname;
    new_node->m.last_heartbeat =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + new_member_heartbeat_slack;

    // Insert it into the correct position
    if (prev == nullptr)
        head = new_node;
    else
        prev->next = new_node;

    lg->debug("Added member at " + hostname + " with id " + std::to_string(id) + " at local time " +
        std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count())  + " to membership list");

    assert(num_members() - original_size == 1);

    return id;
}

// Gets a member from the membership list by ID
auto member_list::get_member_by_id(uint32_t id) const -> member {
    for (node *cur = head; cur != nullptr; cur = cur->next) {
        if (cur->m.id == id) {
            return cur->m;
        }
    }
    return member();
}

// Removes a member from the membership list
void member_list::remove_member(uint32_t id) {
    uint32_t original_size = num_members();

    std::vector<member> initial_neighbors = get_neighbors();

    // First, just remove the member from the list
    node *prev = nullptr;
    node *cur = head;
    while (cur != nullptr) {
        if (cur->m.id == id) {
            if (prev == nullptr) {
                head = cur->next;
            } else {
                prev->next = cur->next;
            }
            lg->debug("Removed member at " + cur->m.hostname + " from list with id " + std::to_string(id));
            delete cur;
            break;
        }

        prev = cur;
        cur = cur->next;
    }

    std::vector<member> new_neighbors = get_neighbors();

    // For the new members, reset their last heartbeat time to be now so that they aren't immediately marked failed
    for (member m : new_neighbors) {
        // If we cannot find this neighbor m in the previous list of neighbors, it is new
        if (std::find_if(initial_neighbors.begin(), initial_neighbors.end(),
                [=](member n) {return n.id == m.id;}) == initial_neighbors.end()) {

            update_heartbeat(m.id);
        }
    }

    assert(original_size - num_members() == 1);
}

// Updates the heartbeat for a member to the current time
void member_list::update_heartbeat(uint32_t id) {
    for (node *cur = head; cur != nullptr; cur = cur->next) {
        if (cur->m.id == id) {
            cur->m.last_heartbeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            return;
        }
    }
}

// Gets a list of the 2 successors and 2 predecessors (or fewer if there are <5 members)
auto member_list::get_neighbors() const -> std::vector<member> {
    std::vector<member> ret;

    node *cur;

    if (!joined_list())
        return ret;

    // Add all the members other than ourselves if the total number of members is <=5
    if (num_members() <= 5) {
        for (cur = head; cur != nullptr; cur = cur->next) {
            if (cur->m.hostname != local_hostname)
                ret.push_back(cur->m);
        }
        return ret;
    }

    // Get the 2 predecessors
    node *prev_2[2] = {nullptr, nullptr};
    node *us = head;
    cur = head;
    while (cur != nullptr) {
        if (cur->m.hostname == local_hostname) {
            us = cur;
            break;
        }

        prev_2[0] = prev_2[1];
        prev_2[1] = cur;
        cur = cur->next;
    }

    // Add them to the list
    for (int i = 0; i < 2; i++) {
        if (prev_2[i] != nullptr)
            ret.push_back(prev_2[i]->m);
    }

    // If either 1 or 2 predecessors were not found we have to check at the end of the list
    int num_preds_not_found = (prev_2[0] == nullptr) + (prev_2[1] == nullptr);
    if (num_preds_not_found > 0) {
        // Find the last 2 elements of the list
        prev_2[0] = nullptr;
        prev_2[1] = nullptr;
        for (cur = head; cur != nullptr; cur = cur->next) {
            prev_2[0] = prev_2[1];
            prev_2[1] = cur;
        }

        // Add either 1 or 2 of those that were found to the list
        for (int i = 0; i < num_preds_not_found; i++) {
            ret.push_back(prev_2[1 - i]->m);
        }
    }

    // Get the 2 successors starting from us
    cur = us;
    for (int i = 0; i < 2; i++) {
        if (cur->next == nullptr)
            cur = head;
        else
            cur = cur->next;

        ret.push_back(cur->m);
    }

    assert(ret.size() == 4);
    for (int i = 0; i < 4; i++) {
        assert(ret[i].hostname != local_hostname);
        assert(ret[i].hostname != "");
        assert(ret[i].id != 0);
    }

    return ret;
}

// Get the number of members total
auto member_list::num_members() const -> uint32_t {
    uint32_t count = 0;
    for (node *cur = head; cur != nullptr; cur = cur->next) {
        count++;
    }
    return count;
}

// Whether or not we are in the member list
auto member_list::joined_list() const -> bool {
    for (node *cur = head; cur != nullptr; cur = cur->next) {
        if (cur->m.hostname == local_hostname)
            return true;
    }
    return false;
}

// Gets a list of all the members
auto member_list::get_members() const -> std::vector<member> {
    std::vector<member> list;
    for (node *cur = head; cur != nullptr; cur = cur->next) {
        list.push_back(cur->m);
    }
    return list;
}

// Gets the successor to the node with the given ID
// Returns a member with ID 0 if the given ID was not found
auto member_list::get_successor(uint32_t id) const -> member {
    for (node *cur = head; cur != nullptr; cur = cur->next) {
        if (cur->m.id == id) {
            if (cur->next == nullptr) {
                // Wrap around to the head for the successor
                return head->m;
            } else {
                return cur->next->m;
            }
        }
    }
    // Node with given ID not found
    return member();
}

auto member::operator==(const member &m) const -> bool {
    return hostname == m.hostname && id == m.id && last_heartbeat == m.last_heartbeat;
}