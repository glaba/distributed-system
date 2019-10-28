#include "heartbeater.h"
#include "utils.h"
#include "logging.h"

#include <chrono>
#include <cstring>
#include <cassert>
#include <iostream>

using namespace std::chrono;

template <bool is_introducer>
heartbeater<is_introducer>::heartbeater(member_list *mem_list_, logger *lg_, udp_client_svc *udp_client_, 
        udp_server_svc *udp_server_, std::string local_hostname_, uint16_t port_)
    : local_hostname(local_hostname_), lg(lg_), udp_client(udp_client_), udp_server(udp_server_), mem_list(mem_list_), port(port_) {

    if (is_introducer) {
        int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        our_id = std::hash<std::string>()(local_hostname) ^ std::hash<int>()(join_time);

        mem_list->add_member(local_hostname, our_id);
        joined_ids.insert(our_id);
        joined_group = true;
    }
}

template <bool is_introducer>
void heartbeater<is_introducer>::start() {
    lg->log("Starting heartbeater");

    std::thread server_thread([this] {server();});
    server_thread.detach();

    std::thread client_thread([this] {client();});
    client_thread.detach();
}

// Returns the list of members of the group that this node is aware of
template <bool is_introducer>
std::vector<member> heartbeater<is_introducer>::get_members() {
    std::lock_guard<std::mutex> guard(member_list_mutex);

    return mem_list->get_members();
}

template <bool is_introducer>
void heartbeater<is_introducer>::client() {
    // Remaining client code here
    while (true) {
        { // Atomic block
            std::lock_guard<std::mutex> guard(member_list_mutex);

            // Check neighbors list for failures
            check_for_failed_neighbors();

            // For each of fails / leaves / joins, send a message out to our neighbors
            std::vector<uint32_t> failed_nodes = failed_nodes_queue.pop();
            std::vector<uint32_t> left_nodes = left_nodes_queue.pop();
            std::vector<member> joined_nodes = joined_nodes_queue.pop();

            char *msg_buf;
            unsigned msg_buf_len;

            // Create the message to send out
            message msg(our_id);
            msg.set_failed_nodes(failed_nodes);
            msg.set_left_nodes(left_nodes);
            msg.set_joined_nodes(joined_nodes);
            msg_buf = msg.serialize(msg_buf_len);
            assert(msg_buf != nullptr);
            assert(msg_buf_len > 0);

            // Send the message out to all the neighbors
            for (auto mem : mem_list->get_neighbors()) {
                if (msg_buf != nullptr) {
                    udp_client->send(mem.hostname, std::to_string(port), msg_buf, msg_buf_len);
                }
            }
            delete[] msg_buf;

            // If we are the introducer, also send the introduction messages
            if (is_introducer && new_nodes_queue.size() > 0) {
                send_introducer_msg();
            }
        }

        // Sleep for heartbeat_interval milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval_ms));
    }
}

// Scans through neighbors and marks those with a heartbeat past the timeout as failed
template <bool is_introducer>
void heartbeater<is_introducer>::check_for_failed_neighbors() {
    uint64_t current_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::vector<member> neighbors = mem_list->get_neighbors();

    for (auto mem : neighbors) {        
        if (current_time - mem.last_heartbeat > timeout_interval_ms) {
            lg->log("Node at " + mem.hostname + " with id " + std::to_string(mem.id) + " timed out!");
            mem_list->remove_member(mem.id);

            // Only tell neighbors about failure if we are still in the group
            // If we are not still in the group, all members will drop out eventually
            if (joined_group.load()) {
                // Tell our neighbors about this failure
                failed_nodes_queue.push(mem.id, message_redundancy);

                // Call the on_fail handler if provided
                if (on_fail_handler) {
                    on_fail_handler(mem);
                }
            }
        }
    }
}

// (Should be called only by introducer) Sends pending messages to newly joined nodes
template <bool is_introducer>
void heartbeater<is_introducer>::send_introducer_msg() {
    char *msg_buf;
    unsigned msg_buf_len;

    std::vector<member> all_members = mem_list->get_members();
    message intro_msg(our_id);
    intro_msg.set_joined_nodes(all_members);
    msg_buf = intro_msg.serialize(msg_buf_len);

    assert(msg_buf != nullptr);
    assert(msg_buf_len > 0);

    for (auto node : new_nodes_queue.pop()) {
        lg->log("Sent introducer message to host at " + node.hostname + " with ID " + std::to_string(node.id));
        udp_client->send(node.hostname, std::to_string(port), msg_buf, msg_buf_len);
    }

    delete[] msg_buf;
}

// Initiates an async request to join the group by sending a message to the introducer
template <bool is_introducer>
void heartbeater<is_introducer>::join_group(std::string introducer) {
    if (is_introducer)
        return;

    lg->log("Requesting introducer to join group");        
    joined_group = true;

    int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    our_id = std::hash<std::string>()(local_hostname) ^ std::hash<int>()(join_time);

    member us;
    us.id = our_id;
    us.hostname = local_hostname;

    char *buf;
    unsigned buf_len;

    message join_req(our_id);
    join_req.set_joined_nodes(std::vector<member>{us});
    buf = join_req.serialize(buf_len);

    assert(buf != nullptr);
    assert(buf_len > 0);

    for (int i = 0; i < message_redundancy; i++)
        udp_client->send(introducer, std::to_string(port), buf, buf_len);
    
    delete[] buf;
}

// Sends a message to peers stating that we are leaving
template <bool is_introducer>
void heartbeater<is_introducer>::leave_group() {
    lg->log("Leaving the group");

    joined_group = false;
    left_nodes_queue.push(our_id, message_redundancy);
}

template <bool is_introducer>
void heartbeater<is_introducer>::server() {
    udp_server->start_server(port);

    int size;
    char buf[1024];

    // Code to listen for messages and handle them accordingly here
    while (true) {
        // If we are not in the group, do not listen for messages
        if (!joined_group.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        // Listen for messages and process each one
        if ((size = udp_server->recv(buf, 1024)) > 0) {
            std::lock_guard<std::mutex> guard(member_list_mutex);

            message msg(buf, size);

            if (!msg.is_well_formed()) {
                lg->log("Received malformed message! Reason: " + msg.why_malformed());
            }
            assert(msg.is_well_formed());

            for (member m : msg.get_joined_nodes()) {
                assert(m.id != 0 && m.hostname != "");

                // Only add and propagate information about this join if we've never seen this node
                if (joined_ids.find(m.id) == joined_ids.end()) {
                    joined_ids.insert(m.id);

                    // If we are the introducer, mark this node to receive the full membership list
                    if (is_introducer) {
                        lg->log("Received request to join group from (" + m.hostname + ", " + std::to_string(m.id) + ")");
                        new_nodes_queue.push(m, message_redundancy);
                    }

                    mem_list->add_member(m.hostname, m.id);
                    joined_nodes_queue.push(m, message_redundancy);

                    if (on_join_handler) {
                        on_join_handler(m);
                    }
                }
            }

            for (uint32_t id : msg.get_left_nodes()) {
                // Only propagate this message if the member has not yet been removed
                if (mem_list->get_member_by_id(id).id == id) {
                    mem_list->remove_member(id);
                    left_nodes_queue.push(id, message_redundancy);

                    if (on_leave_handler) {
                        on_leave_handler(mem_list->get_member_by_id(id));
                    }
                }
            }

            for (uint32_t id : msg.get_failed_nodes()) {
                // Only propagate this message if the member has not yet been removed
                if (mem_list->get_member_by_id(id).id == id) {
                    mem_list->remove_member(id);
                    failed_nodes_queue.push(id, message_redundancy);

                    if (on_fail_handler) {
                        on_fail_handler(mem_list->get_member_by_id(id));
                    }
                }
            }

            mem_list->update_heartbeat(msg.get_id());
        }
    }

    udp_server->stop_server();
}

// Force the compiler to compile both templates
template class heartbeater<true>;
template class heartbeater<false>;
