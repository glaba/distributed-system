#include "member_list.h"
#include "logging.h"

#include <algorithm>

member create_member(std::string hostname, uint32_t id) {
    member cur;
    cur.id = id;
    cur.hostname = hostname;
    cur.last_heartbeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return cur;
}

// Adds ourselves as the first introducer and returns our ID
uint32_t member_list::add_self_as_introducer(std::string hostname, int join_time) {
    member m = create_member(hostname, std::hash<std::string>()(hostname) ^ std::hash<int>()(join_time));
    list.insert(list.end(), m);
    log("Added self to membership list as introducer at local time " + std::to_string(join_time));
    return m.id;
}

// Adds a member to the membership list using hostname and ID
uint32_t member_list::add_member(std::string hostname, uint32_t id) {
    log_v("Received message to add member at " + hostname + " with id " + std::to_string(id));

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

    auto new_it = list.insert(it, m);

    // Check if the new member is ourself and note it down if so
    if (hostname == local_hostname) {
        self = new_it;
    }

    log("Added member at " + hostname + " with id " + std::to_string(id) + " at local time " + 
        std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count())  + " to membership list");

    return m.id;
}


// Removes a member from the membership list
void member_list::remove_member(uint32_t id) {
    std::vector<member> initial_neighbors = get_neighbors();

    for (auto it = list.begin(); it != list.end(); it++) {
        if (it->id == id) {
            list.erase(it);
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
            log_v("Detected heartbeat from node at " + it->hostname + " with id " + std::to_string(id));
            it->last_heartbeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }
    }
}

// Gets a list of the 2 successors and 2 predecessors (or fewer if there are <5 members)
std::vector<member> member_list::get_neighbors() {
    std::vector<member> neighbors;

    if (local_hostname == "") {
        return neighbors;
    }

    auto forward_it = self;
    for (int i = 0; i < 2; i++) {
        forward_it++;
        if (forward_it == list.end()) {
            forward_it = list.begin();
        }

        if (forward_it == self)
            break;

        neighbors.push_back(*forward_it);
    }

    auto backward_it = self;
    for (int i = 0; i < 2; i++) {
        if (backward_it == list.begin()) {
            backward_it = list.end();
        }
        backward_it--;

        if (backward_it == self || backward_it == forward_it)
            break;

        neighbors.push_back(*backward_it);
    }

    return neighbors;
}

// Get the number of members total
uint32_t member_list::num_members() {
    return list.size();
}

std::list<member> member_list::__get_internal_list() {
    return list;
}

bool member::operator==(const member &m) {
    return hostname == m.hostname && id == m.id && last_heartbeat == m.last_heartbeat;
}
