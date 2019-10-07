#include "heartbeat.h"
#include "utils.h"
#include "logging.h"

#include <chrono>
#include <cstring>
#include <iostream>

using namespace std::chrono;

int heartbeater::message_redundancy = 4;
uint64_t heartbeater::heartbeat_interval_ms = 250;
uint64_t heartbeater::timeout_interval_ms = 1000;

heartbeater::heartbeater(member_list mem_list_, logger *lg_, udp_client_svc *udp_client_, udp_server_svc *udp_server_,
        std::string local_hostname_, uint16_t port_)
    : mem_list(mem_list_), lg(lg_), udp_client(udp_client_), udp_server(udp_server_),
      local_hostname(local_hostname_), is_introducer(true), introducer(""), port(port_) {

    int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    our_id = std::hash<std::string>()(local_hostname) ^ std::hash<int>()(join_time);

    mem_list.add_member(local_hostname, our_id);
    joined_ids.insert(our_id);
}

heartbeater::heartbeater(member_list mem_list_, logger *lg_, udp_client_svc *udp_client_, udp_server_svc *udp_server_,
        std::string local_hostname_, bool is_introducer_, std::string introducer_, uint16_t port_)
    : mem_list(mem_list_), lg(lg_), udp_client(udp_client_), udp_server(udp_server_),
      local_hostname(local_hostname_), is_introducer(is_introducer_), introducer(introducer_), port(port_) {

    int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    our_id = std::hash<std::string>()(local_hostname) ^ std::hash<int>()(join_time);
}

void heartbeater::start() {
    lg->log("Starting heartbeater");

    std::thread server_thread([this] {server();});
    server_thread.detach();

    std::thread client_thread([this] {client();});
    client_thread.detach();

    // Here we will have a command prompt if logging to a file
    if (!lg->using_stdout()) {
        std::cout << "m for membership list" << std::endl;
        std::cout << "i for our ID" << std::endl;
        std::cout << "j to join the group" << std::endl;
        std::cout << "l to leave the group" << std::endl;
    }
    bool already_joined = false;
    while (true) {
        if (lg->using_stdout()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        } else {
            std::cout << "> ";
            std::string input;
            std::cin >> input;

            if (input == "m") {
                mem_list.print();
            } else if (input == "i") {
                std::cout << our_id << std::endl;
            } else if (input == "j") {
                if (already_joined) {
                    std::cout << "Already joined" << std::endl;
                } else {
                    std::lock_guard<std::mutex> guard(member_list_mutex);
                    if (introducer != "") {
                        std::cout << "Joining..." << std::endl;
                        join_group(introducer);
                    } else {
                        std::cout << "We are the introducer" << std::endl;
                    }
                }
            } else if (input == "l") {
                std::lock_guard<std::mutex> guard(member_list_mutex);
                std::cout << "Leaving..." << std::endl;
                add_leave_msg_to_list(our_id);
                break;
            } else {
                std::cout << "Invalid command" << std::endl;
            }
        }
    }
}

void heartbeater::client() {
    // if (introducer != "") {
    //     join_group(introducer);
    // }

    // Remaining client code here
    while (true) {
        {
            std::lock_guard<std::mutex> guard(member_list_mutex);

            // Get list of neighbors
            uint64_t current_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            std::vector<member> neighbors = mem_list.get_neighbors();

            // comb neighbors list to see if any nodes failed
            for (auto mem : neighbors) {
                if (current_time - mem.last_heartbeat > timeout_interval_ms) {
                    mem_list.remove_member(mem.id);
                    add_fail_msg_to_list(mem.id);
                }
            }

            // the following vector constructions iterate over the respective "message queues"
            // and append messages details to the vector (e.g. ids) - they then decrement the amount
            // of messages that still need to be sent, and will erase entries in the queue that are finished

            std::vector<uint32_t> failed_nodes;
            // @TODO: THIS SHOULD BE THREAD SAFE
            auto k = std::begin(failed_nodes_counts);
            while (k != std::end(failed_nodes_counts)) {
                auto id_cnt_tup = *k;
                failed_nodes.push_back(std::get<0>(id_cnt_tup));
                std::get<1>(*k) = std::get<1>(id_cnt_tup) - 1;

                if (std::get<1>(id_cnt_tup) <= 0) {
                    k = failed_nodes_counts.erase(k);
                } else {
                    ++k;
                }
            }

            std::vector<uint32_t> left_nodes;
            // @TODO: THIS SHOULD BE THREAD SAFE
            auto i = std::begin(left_nodes_counts);
            while (i != std::end(left_nodes_counts)) {
                auto id_cnt_tup = *i;
                left_nodes.push_back(std::get<0>(id_cnt_tup));
                std::get<1>(*i) = std::get<1>(id_cnt_tup) - 1;

                if (std::get<1>(id_cnt_tup) <= 0) {
                    i = left_nodes_counts.erase(i);
                } else {
                    ++i;
                }
            }

            std::vector<member> joined_nodes;
            // @TODO: THIS SHOULD BE THREAD SAFE
            auto j = std::begin(joined_nodes_counts);
            while (j != std::end(joined_nodes_counts)) {
                auto mem_cnt_tup = *j;
                joined_nodes.push_back(std::get<0>(mem_cnt_tup));
                std::get<1>(*j) = std::get<1>(mem_cnt_tup) - 1;

                if (std::get<1>(mem_cnt_tup) <= 0) {
                    j = joined_nodes_counts.erase(j);
                } else {
                    ++j;
                }
            }

            unsigned length;
            char *msg = construct_msg(failed_nodes, left_nodes, joined_nodes, &length);
            for (auto mem : neighbors) {
                udp_client->send(mem.hostname, std::to_string(port), msg, length);
            }

            delete[] msg;

            if (is_introducer) {
                auto it = new_node_introduction_counts.begin();
                while (it != new_node_introduction_counts.end()) {
                    auto intro = *it;

                    udp_client->send(mem_list.get_member_by_id(std::get<2>(intro)).hostname,
                    std::to_string(port), std::get<0>(intro), std::get<1>(intro));

                    // Remove the item from the vector if the TTL has expired
                    std::get<3>(intro) = std::get<3>(intro) - 1;
                    if (std::get<3>(intro) == 0) {
                        delete std::get<0>(intro); // Free the memory for the message
                        it = new_node_introduction_counts.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        // sleep for heartbeat_interval milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval_ms));
    }
}

// Creates a message that represents the provided set of failed / left / joined nodes
char *heartbeater::construct_msg(std::vector<uint32_t> failed_nodes, std::vector<uint32_t> left_nodes, std::vector<member> joined_nodes, unsigned *length) {
    unsigned size =
        sizeof(uint32_t) + // ID
        sizeof('L') + sizeof(uint32_t) + sizeof(uint32_t) * failed_nodes.size() + // L<num fails><failed IDs>
        sizeof('L') + sizeof(uint32_t) + sizeof(uint32_t) * left_nodes.size() + // L<num leaves><left IDs>
        sizeof('J') + sizeof(uint32_t); // J<num joins> ... more to come
    for (member m : joined_nodes) {
        size += m.hostname.length() + sizeof('\0') + sizeof(uint32_t); // <hostname>\0<ID>
    }

    char *buf_start = new char[size];
    char *buf = buf_start;

    *reinterpret_cast<uint32_t*>(buf) = our_id; // ID
    buf += sizeof(uint32_t);

    *buf++ = 'L'; // L
    *reinterpret_cast<uint32_t*>(buf) = failed_nodes.size(); // <num fails>
    buf += sizeof(uint32_t);
    for (uint32_t id : failed_nodes) {
        *reinterpret_cast<uint32_t*>(buf) = id; // <failed ID>
        buf += sizeof(uint32_t);
    }

    *buf++ = 'L'; // L
    *reinterpret_cast<uint32_t*>(buf) = left_nodes.size(); // <num leaves>
    buf += sizeof(uint32_t);
    for (uint32_t id : left_nodes) {
        *reinterpret_cast<uint32_t*>(buf) = id; // <left ID>
        buf += sizeof(uint32_t);
    }

    *buf++ = 'J'; // J
    *reinterpret_cast<uint32_t*>(buf) = joined_nodes.size(); // <num joins>
    buf += sizeof(uint32_t);
    for (member m : joined_nodes) {
        for (unsigned i = 0; i < m.hostname.length(); i++) {
            *buf++ = m.hostname[i]; // <hostname>
        }
        *buf++ = '\0'; // \0
        *reinterpret_cast<uint32_t*>(buf) = m.id; // <joined ID>
        buf += sizeof(uint32_t);
    }

    *length = size;
    return buf_start;
}

// Initiates an async request to join the group by sending a message to the introducer
void heartbeater::join_group(std::string introducer) {
    lg->log("Requesting introducer to join group");

    member us;
    us.id = our_id;
    us.hostname = local_hostname;

    unsigned length;
    char *msg = construct_msg(std::vector<uint32_t>(), std::vector<uint32_t>(), std::vector<member>{us}, &length);

    udp_client->send(introducer, std::to_string(port), msg, length);

    delete[] msg;
}

bool heartbeater::has_joined() {
    return mem_list.num_members() > 0;
}

void heartbeater::server() {
    udp_server->start_server(port);

    int size;
    char buf[1024];

    // Code to listen for messages and handle them accordingly here
    while (true) {
        // Listen for messages and process each one
        if ((size = udp_server->recv(buf, 1024)) > 0) {
            std::lock_guard<std::mutex> guard(member_list_mutex);

            uint32_t id = *reinterpret_cast<uint32_t*>(buf);
            mem_list.update_heartbeat(id);

            char *cur_pos = buf + sizeof(id);
            cur_pos += process_fail_msg(cur_pos);
            cur_pos += process_leave_msg(cur_pos);
            cur_pos += process_join_msg(cur_pos);

            memset(buf, '\0', 1024);
        }
    }

    udp_server->stop_server();
}

// Processes a fail message (L), updates the member table, and returns the number of bytes consumed
unsigned heartbeater::process_fail_msg(char *buf) {
    int i = 0;

    // Increment past the 'L'
    i++;

    uint32_t num_fails = *reinterpret_cast<uint32_t*>(buf + i);
    i += sizeof(num_fails);

    for (uint32_t j = 0; j < num_fails; j++) {
        uint32_t id = *reinterpret_cast<uint32_t*>(buf + i);

        // Only propagate the message if the member has not yet been removed
        if (mem_list.get_member_by_id(id).id == id)
            add_fail_msg_to_list(id);

        lg->log("Member at hostname " + mem_list.get_member_by_id(id).hostname + " failed");
        mem_list.remove_member(*reinterpret_cast<uint32_t*>(buf + i));
        i += sizeof(uint32_t);
    }

    return i;
}

// Processes a leave message (L), updates the member table, and returns the number of bytes consumed
unsigned heartbeater::process_leave_msg(char *buf) {
    int i = 0;

    // Increment past the 'L'
    i++;

    uint32_t num_leaves = *reinterpret_cast<uint32_t*>(buf + i);
    i += sizeof(num_leaves);

    for (uint32_t j = 0; j < num_leaves; j++) {
        uint32_t id = *reinterpret_cast<uint32_t*>(buf + i);

        // Only propagate the message if the member has not yet been removed
        if (mem_list.get_member_by_id(id).id == id)
            add_leave_msg_to_list(id);

        lg->log("Member at hostname " + mem_list.get_member_by_id(id).hostname + " left");
        mem_list.remove_member(*reinterpret_cast<uint32_t*>(buf + i));
        i += sizeof(uint32_t);
    }

    return i;
}

// Processes a join message (J), updates the member table, and returns the number of bytes consumed
unsigned heartbeater::process_join_msg(char *buf) {
    int i = 0;

    // Increment past the 'J'
    i++;

    uint32_t num_joins = *reinterpret_cast<uint32_t*>(buf + i);
    i += sizeof(num_joins);

    for (uint32_t j = 0; j < num_joins; j++) {
        std::string hostname = "";

        for (; buf[i] != '\0'; i++) {
            hostname += buf[i];
        }
        i++; // Increment past the \0

        uint32_t id = *reinterpret_cast<uint32_t*>(buf + i);
        i += sizeof(num_joins);

        // Only propagate the message if the member has not yet joined
        // and if we have never seen the member before (to prevent leave/join cycles)
        if (mem_list.get_member_by_id(id).id == 0 && (joined_ids.find(id) == joined_ids.end())) {
            mem_list.add_member(hostname, id);
            joined_ids.insert(id);

            add_join_msg_to_list(id);

            if (is_introducer) {
                // Send entire membership list to this node if we are the introducer
                unsigned length;
                char *msg = construct_msg(std::vector<uint32_t>(), std::vector<uint32_t>(), mem_list.get_members(), &length);
                new_node_introduction_counts.push_back(std::make_tuple(msg, length, id, message_redundancy));
            }
        }
    }

    return i;
}

void heartbeater::add_fail_msg_to_list(uint32_t id) {
    // iterate through the queue of fail msgs
    failed_nodes_counts.push_back(std::make_tuple(id, message_redundancy));
}

void heartbeater::add_leave_msg_to_list(uint32_t id) {
    // iterate through the queue of leave msgs
    left_nodes_counts.push_back(std::make_tuple(id, message_redundancy));
}

void heartbeater::add_join_msg_to_list(uint32_t id) {
    // iterate through the queue of join msgs
    joined_nodes_counts.push_back(std::make_tuple(mem_list.get_member_by_id(id), message_redundancy));
}
