#include "member_list.h"

#include <algorithm>
#include <iostream>

member create_member(std::string hostname, uint32_t id) {
    member cur;
    cur.id = id;
    cur.hostname = hostname;
    cur.last_heartbeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return cur;
}

// Adds a member to the membership list using hostname and ID
uint32_t member_list::add_member(std::string hostname, uint32_t id) {
    member m = create_member(hostname, id);

    // First check if the member already exists
    for (auto i = list.begin(); i != list.end(); i++) {
        if (m.id == i->id) {
            return m.id;
        }
    }

    auto it = list.begin();
    for (; it != list.end(); it++) {
        if (m.id < it->id) {
            break;
        }
    }

    list.insert(it, m);

    lg->log("Added member at " + hostname + " with id " + std::to_string(id) + " at local time " +
        std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count())  + " to membership list");

    return m.id;
}


// Removes a member from the membership list
void member_list::remove_member(uint32_t id) {
    std::vector<member> initial_neighbors = get_neighbors();

    for (auto it = list.begin(); it != list.end(); it++) {
        if (it->id == id) {
            list.erase(it);
            lg->log("Removed member at " + it->hostname + " from list with id " + std::to_string(id));
            break;
        }
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
}

// Updates the heartbeat for a member to the current time
void member_list::update_heartbeat(uint32_t id) {
    for (auto it = list.begin(); it != list.end(); it++) {
        if (it->id == id) {
            lg->log_v("Detected heartbeat from node at " + it->hostname + " with id " + std::to_string(id));
            it->last_heartbeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }
    }
}

// Gets a list of the 2 successors and 2 predecessors (or fewer if there are <5 members)
std::vector<member> member_list::get_neighbors() {
    std::vector<member> neighbors;

    if (list.empty()) return neighbors;

    if (local_hostname == "") {
        return neighbors;
    }

    auto forward_it = std::find_if(list.begin(), list.end(),
        [=] (member m) {return m.hostname == local_hostname;});

    std::string stop_hostname;
    for (int i = 0; i < 2; i++) {
        forward_it++;
        if (forward_it == list.end()) {
            forward_it = list.begin();
        }

        stop_hostname = forward_it->hostname;

        if (forward_it->hostname == local_hostname)
            break;

        neighbors.push_back(*forward_it);
    }

    auto backward_it = std::find_if(list.begin(), list.end(),
        [=] (member m) {return m.hostname == local_hostname;});

    for (int i = 0; i < 2; i++) {
        if (backward_it == list.begin()) {
            backward_it = list.end();
        }
        backward_it--;

        if (backward_it->hostname == local_hostname || backward_it->hostname == stop_hostname)
            break;

        if (neighbors.size() != 1) {
            // if the neighbors list is of size one, this would add the same element twice
            neighbors.push_back(*backward_it);
        }
    }

    return neighbors;
}

member member_list::get_member_by_id(uint32_t id) {
    for (auto it = list.begin(); it != list.end(); it++) {
        if (it->id == id) {
            return *it;
        }
    }
    return member();
}

// Get the number of members total
uint32_t member_list::num_members() {
    return list.size();
}

// Gets a list of all the members (to be used by introducer)
std::vector<member> member_list::get_members() {
    std::vector<member> members;
    for (auto it = list.begin(); it != list.end(); it++) {
        members.push_back(*it);
    }
    return members;
}

std::list<member> member_list::__get_internal_list() {
    return list;
}

bool member::operator==(const member &m) {
    return hostname == m.hostname && id == m.id && last_heartbeat == m.last_heartbeat;
}
