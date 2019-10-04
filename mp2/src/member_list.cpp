#include "member_list.h"
#include "logging.h"

#include <chrono>
#include <ctime>

using namespace std::chrono;

member create_member(std::string hostname, int join_time) {
    member cur;
    cur.id = std::hash<std::string>()(hostname) ^ std::hash<int>()(join_time);
    cur.hostname = hostname;
    cur.last_heartbeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return cur;
}

// Adds ourselves as the first introducer and returns our ID
uint32_t member_list::add_self_as_introducer(std::string hostname, int join_time) {
    std::lock_guard<std::mutex> guard(member_list_mutex);

    member m = create_member(hostname, join_time);
    list.insert(list.end(), m);
    log("Added self to membership list as introducer at local time " + std::to_string(join_time));
    return m.id;
}

// Adds a member to the membership list and returns their ID
uint32_t member_list::add_member(std::string hostname, int join_time) {
    std::lock_guard<std::mutex> guard(member_list_mutex);

    member m = create_member(hostname, join_time);
    auto it = list.begin();
    for (; it != list.end(); it++) {
        if (m.id > it->id) {
            break;
        }
    }

    auto new_it = list.insert(it, m);

    // Check if the new member is ourself and note it down if so
    if (hostname == local_hostname) {
        self = new_it;
    }

    log("Added member at " + hostname + " with remote join time " + std::to_string(join_time) + " to membership list");
    return m.id;
}

// Removes a member from the membership list
void member_list::remove_member(uint32_t id) {
    std::lock_guard<std::mutex> guard(member_list_mutex);

    for (auto it = list.begin(); it != list.end(); it++) {
        if (id == it->id) {
            list.erase(it);
            return;
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

        if (backward_it == self)
            break;

        neighbors.push_back(*backward_it);
    }

    return neighbors;
}
