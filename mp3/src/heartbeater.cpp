#include "heartbeater.h"
#include "logging.h"

#include <chrono>
#include <cstring>
#include <cassert>
#include <iostream>
#include <algorithm>

using namespace std::chrono;
using std::unique_ptr;
using std::make_unique;

template <bool is_introducer_>
heartbeater<is_introducer_>::heartbeater(member_list *mem_list_, logger *lg_, udp_client_intf *client_,
        udp_server_intf *server_, std::string local_hostname_, uint16_t port_)
    : local_hostname(local_hostname_), lg(lg_), client(client_), server(server_),
      nodes_can_join(true), mem_list(mem_list_), port(port_) {

    our_id = 0;

    joined_group = false;
    if (is_introducer_) {
        int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        our_id = std::hash<std::string>()(local_hostname) ^ std::hash<int>()(join_time);

        mem_list->add_member(local_hostname, our_id);
        joined_ids.insert(our_id);
        joined_group = true;
    }
}

// Starts the heartbeater
template <bool is_introducer_>
void heartbeater<is_introducer_>::start() {
    lg->log("Starting heartbeater");

    running = true;

    server_thread = make_unique<std::thread>([this] {server_thread_function();});
    server_thread->detach();

    client_thread = make_unique<std::thread>([this] {client_thread_function();});
    client_thread->detach();
}

// Stops the heartbeater synchronously
template <bool is_introducer_>
void heartbeater<is_introducer_>::stop() {
    lg->log("Stopping heartbeater");

    server->stop_server();
    running = false;

    // Sleep for enough time for the threads to stop and then delete them
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

// Returns the list of members of the group that this node is aware of
template <bool is_introducer_>
std::vector<member> heartbeater<is_introducer_>::get_members() {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);

    return mem_list->get_members();
}

// Gets the member object corresponding to the provided ID
template <bool is_introducer_>
member heartbeater<is_introducer_>::get_member_by_id(uint32_t id) {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);

    return mem_list->get_member_by_id(id);
}

// Returns the list of members of the group that this node is aware of
template <bool is_introducer_>
member heartbeater<is_introducer_>::get_successor() {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);

    return mem_list->get_successor(our_id);
}

// Client thread function
template <bool is_introducer_>
void heartbeater<is_introducer_>::client_thread_function() {
    // Remaining client code here
    while (true) {
        // Exit the loop and the thread if the heartbeater is stopped
        if (!running.load()) {
            break;
        }

        { // Atomic block
            std::lock_guard<std::recursive_mutex> guard(member_list_mutex);

            // Check neighbors list for failures
            check_for_failed_neighbors();

            // For each of fails / leaves / joins, send a message out to our neighbors
            std::vector<uint32_t> failed_nodes = failed_nodes_queue.pop();
            std::vector<uint32_t> left_nodes = left_nodes_queue.pop();
            std::vector<member> joined_nodes = joined_nodes_queue.pop();

            unique_ptr<char[]> msg_buf;
            unsigned msg_buf_len;

            // Create the message to send out
            hb_message msg(our_id);
            msg.set_failed_nodes(failed_nodes);
            msg.set_left_nodes(left_nodes);
            msg.set_joined_nodes(joined_nodes);
            msg_buf = msg.serialize(msg_buf_len);
            assert(msg_buf_len > 0);

            // Send the message out to all the neighbors
            for (auto mem : mem_list->get_neighbors()) {
                if (msg_buf != nullptr) {
                    client->send(mem.hostname, port, msg_buf.get(), msg_buf_len);
                }
            }

            // If we are the introducer, also send the introduction messages
            if (is_introducer_ && nodes_can_join.load() && new_nodes_queue.size() > 0) {
                send_introducer_msg();
            }
        }

        // Sleep for heartbeat_interval milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval_ms));
    }
}

// Scans through neighbors and marks those with a heartbeat past the timeout as failed
template <bool is_introducer_>
void heartbeater<is_introducer_>::check_for_failed_neighbors() {
    uint64_t current_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::vector<member> neighbors = mem_list->get_neighbors();

    for (auto mem : neighbors) {
        if (current_time > mem.last_heartbeat + timeout_interval_ms) {
            lg->log("Node at " + mem.hostname + " with id " + std::to_string(mem.id) + " timed out!");
            mem_list->remove_member(mem.id);

            // Only tell neighbors about failure if we are still in the group
            // If we are not still in the group, all members will drop out eventually
            if (joined_group.load()) {
                // Tell our neighbors about this failure
                failed_nodes_queue.push(mem.id, message_redundancy);

                // Call the on_fail handler if provided
                for (auto handler : on_fail_handlers) {
                    handler(mem);
                }
            }
        }
    }
}

// (Should be called only by introducer) Sends pending messages to newly joined nodes
template <bool is_introducer_>
void heartbeater<is_introducer_>::send_introducer_msg() {
    unique_ptr<char[]> msg_buf;
    unsigned msg_buf_len;

    std::vector<member> all_members = mem_list->get_members();
    hb_message intro_msg(our_id);
    intro_msg.set_joined_nodes(all_members);
    msg_buf = intro_msg.serialize(msg_buf_len);
    assert(msg_buf_len > 0);

    auto new_nodes = new_nodes_queue.pop();
    auto updated = new_nodes_queue.peek();

    for (auto node : new_nodes) {
        lg->log("Sent introducer message to host at " + node.hostname + " with ID " + std::to_string(node.id));
        client->send(node.hostname, port, msg_buf.get(), msg_buf_len);

        // If this node isn't in new_nodes_queue anymore, we should now add it to joined_nodes_queue
        // so that it can be added to other nodes' membership lists
        if (std::find_if(updated.begin(), updated.end(), [=](member m) {return m.id == node.id;}) == updated.end()) {
            joined_nodes_queue.push(node, message_redundancy);
        }
    }
}

// Runs the provided function atomically with any functions that read or write to the membership list
template <bool is_introducer_>
void heartbeater<is_introducer_>::run_atomically_with_mem_list(std::function<void()> fn) {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);
    fn();
}

// Initiates an async request to join the group by sending a message to the introducer
template <bool is_introducer_>
void heartbeater<is_introducer_>::join_group(std::string introducer) {
    if (is_introducer_)
        return;

    lg->log("Requesting introducer to join group");
    joined_group = true;

    int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    our_id = std::hash<std::string>()(local_hostname) ^ std::hash<int>()(join_time);

    member us;
    us.id = our_id;
    us.hostname = local_hostname;

    unique_ptr<char[]> buf;
    unsigned buf_len;

    hb_message join_req(our_id);
    join_req.set_joined_nodes(std::vector<member>{us});
    buf = join_req.serialize(buf_len);
    assert(buf_len > 0);

    for (int i = 0; i < message_redundancy; i++) {
        client->send(introducer, port, buf.get(), buf_len);
    }
}

// Sends a message to peers stating that we are leaving
template <bool is_introducer_>
void heartbeater<is_introducer_>::leave_group() {
    lg->log("Leaving the group");

    joined_group = false;
    left_nodes_queue.push(our_id, message_redundancy);
}

// Server thread function
template <bool is_introducer_>
void heartbeater<is_introducer_>::server_thread_function() {
    lg->log("Starting server thread");
    server->start_server(port);

    int size;
    char buf[1024];

    // Code to listen for messages and handle them accordingly here
    while (true) {
        // If the server is not running, stop everything and exit
        if (!running.load()) {
            lg->log("Exiting server thread");
            break;
        }

        // If we are not in the group, do not listen for messages
        if (!joined_group.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        // Listen for messages and process each one
        if ((size = server->recv(buf, 1024)) > 0) {
            std::lock_guard<std::recursive_mutex> guard(member_list_mutex);

            hb_message msg(buf, size);

            if (!msg.is_well_formed()) {
                lg->log("Received malformed heartbeat message!");
                continue;
            }

            // If we are in the group and the message is not from within the group, ignore it (with special case for introducer)
            if (mem_list->get_member_by_id(our_id).id == our_id &&
                mem_list->get_member_by_id(msg.get_id()).id != msg.get_id()) {

                // Allow the message to be processed if we are the introducer and it's a join request
                if (is_introducer_ && msg.get_joined_nodes().size() == 1 &&
                                      msg.get_left_nodes().size() == 0 &&
                                      msg.get_failed_nodes().size() == 0 &&
                                      msg.get_joined_nodes()[0].id == msg.get_id()) {
                    if (nodes_can_join.load()) {
                        // Do nothing
                    } else {
                        lg->log("Nodes cannot join because leader election is occurring!");
                        continue;
                    }
                } else {
                    lg->log("Ignoring heartbeat message from " + std::to_string(msg.get_id()) + " because they are not in the group");
                    continue;
                }
            }

            for (member m : msg.get_joined_nodes()) {
                assert(m.id != 0 && m.hostname != "");

                // Only add and propagate information about this join if we've never seen this node
                if (joined_ids.find(m.id) == joined_ids.end()) {
                    joined_ids.insert(m.id);
                    mem_list->add_member(m.hostname, m.id);

                    if (is_introducer_) {
                        lg->log("Received request to join group from (" + m.hostname + ", " + std::to_string(m.id) + ")");
                        new_nodes_queue.push(m, message_redundancy);
                    } else {
                        joined_nodes_queue.push(m, message_redundancy);
                    }

                    for (auto handler : on_join_handlers) {
                        handler(m);
                    }
                }
            }

            for (uint32_t id : msg.get_left_nodes()) {
                // Only propagate this message if the member has not yet been removed
                member mem = mem_list->get_member_by_id(id);
                if (mem.id == id) {
                    mem_list->remove_member(id);
                    left_nodes_queue.push(id, message_redundancy);

                    for (auto handler : on_leave_handlers) {
                        handler(mem);
                    }
                }
            }

            for (uint32_t id : msg.get_failed_nodes()) {
                // Only propagate this message if the member has not yet been removed
                member mem = mem_list->get_member_by_id(id);
                if (mem.id == id) {
                    mem_list->remove_member(id);
                    failed_nodes_queue.push(id, message_redundancy);

                    for (auto handler : on_fail_handlers) {
                        handler(mem);
                    }
                }
            }

            mem_list->update_heartbeat(msg.get_id());
        }
    }
}

// Adds a handler to the list of handlers that will be called when a node fails
template <bool is_introducer_>
void heartbeater<is_introducer_>::on_fail(std::function<void(member)> handler) {
    // Acquire mutex to prevent concurrent modification of vector
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);
    on_fail_handlers.push_back(handler);
}

// Adds a handler to the list of handlers that will be called when a node leaves
template <bool is_introducer_>
void heartbeater<is_introducer_>::on_leave(std::function<void(member)> handler) {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);
    on_leave_handlers.push_back(handler);
}

// Adds a handler to the list of handlers that will be called when a node joins
template <bool is_introducer_>
void heartbeater<is_introducer_>::on_join(std::function<void(member)> handler) {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);
    on_join_handlers.push_back(handler);
}

// Force the compiler to compile both templates
template class heartbeater<true>;
template class heartbeater<false>;
