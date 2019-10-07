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

heartbeater::heartbeater(member_list mem_list_, udp_client_svc *udp_client_, udp_server_svc *udp_server_,
        std::string local_hostname_, uint16_t port_)
    : mem_list(mem_list_), udp_client(udp_client_), udp_server(udp_server_),
      local_hostname(local_hostname_), port(port_) {
    is_introducer = true;

    int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    our_id = std::hash<std::string>()(local_hostname) ^ std::hash<int>()(join_time);
}

heartbeater::heartbeater(member_list mem_list_, udp_client_svc *udp_client_, udp_server_svc *udp_server_,
        std::string local_hostname_, std::string introducer_, uint16_t port_)
    : mem_list(mem_list_), udp_client(udp_client_), udp_server(udp_server_),
      local_hostname(local_hostname_), introducer(introducer_), port(port_) {
    is_introducer = false;

    int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    our_id = std::hash<std::string>()(local_hostname) ^ std::hash<int>()(join_time);
}

void heartbeater::start() {
    std::thread server_thread([this] {server();});
    server_thread.detach();

    client();
}

void heartbeater::client() {
    if (!is_introducer) {
        join_group(introducer);
    }

    // Remaining client code here
    while (true) {
        std::lock_guard<std::mutex> guard(member_list_mutex);
        
        // Get list of neighbors
        uint64_t current_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        std::vector<member> neighbors = mem_list.get_neighbors();

        // comb neighbors list to see if any nodes failed
        for (auto mem : neighbors) {
            if (current_time - mem.last_heartbeat > timeout_interval_ms) {
                failed_nodes_counts.push_back(std::make_tuple(mem.id, message_redundancy));
                // failed_nodes.push_back(mem.id);
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
            std::get<1>(id_cnt_tup) = std::get<1>(id_cnt_tup) - 1;

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
            std::get<1>(id_cnt_tup) = std::get<1>(id_cnt_tup) - 1;

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
            std::get<1>(mem_cnt_tup) = std::get<1>(mem_cnt_tup) - 1;

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

        delete msg;

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
    return buf;
}

// Initiates an async request to join the group by sending a message to the introducer
void heartbeater::join_group(std::string introducer) {
    member us;
    us.id = our_id;
    us.hostname = local_hostname;

    unsigned length;
    char *msg = construct_msg(std::vector<uint32_t>(), std::vector<uint32_t>(), std::vector<member>{us}, &length);
    udp_client->send(introducer, std::to_string(port), msg, length);

    delete msg;
}

bool heartbeater::has_joined() {
    return mem_list.num_members() > 0;
}

void heartbeater::server() {
    udp_server->start_server(port);

    char buf[1024];

    // Code to listen for messages and handle them accordingly here
    while (true) {
        // Listen for messages and process each one
        if (udp_server->recv(buf, 1024) > 0) {
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
    // @TODO: fail message processing
    int i = 0;

    // Increment past the 'L'
    i++;

    uint8_t num_leaves = buf[i];
    i += sizeof(num_leaves);

    for (uint32_t j = 0; j < num_leaves; j++) {
        mem_list.remove_member(*reinterpret_cast<uint32_t*>(buf + i));
        add_fail_msg_to_list(*reinterpret_cast<uint32_t*>(buf + i));
        i += sizeof(uint32_t);
    }

    return i;
}

// Processes a leave message (L), updates the member table, and returns the number of bytes consumed
unsigned heartbeater::process_leave_msg(char *buf) {
    int i = 0;

    // Increment past the 'L'
    i++;

    uint8_t num_leaves = buf[i];
    i += sizeof(num_leaves);

    for (uint32_t j = 0; j < num_leaves; j++) {
        mem_list.remove_member(*reinterpret_cast<uint32_t*>(buf + i));
        add_leave_msg_to_list(*reinterpret_cast<uint32_t*>(buf + i));
        i += sizeof(uint32_t);
    }

    return i;
}

// Processes a join message (J), updates the member table, and returns the number of bytes consumed
unsigned heartbeater::process_join_msg(char *buf) {
    int i = 0;

    // Increment past the 'J'
    i++;

    uint8_t num_joins = buf[i];
    i += sizeof(num_joins);

    for (uint32_t j = 0; j < num_joins; j++) {
        std::string hostname = "";

        for (; buf[i] != '\0'; i++) {
            hostname += buf[i];
        }
        i++; // Increment past the \0

        uint32_t id = *reinterpret_cast<uint32_t*>(buf + i);
        i += sizeof(num_joins);

        mem_list.add_member(hostname, id);
        add_join_msg_to_list(id);
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
