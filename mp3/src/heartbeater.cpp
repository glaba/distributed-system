#include "heartbeater.h"
#include "heartbeater.hpp"
#include "logging.h"

#include <chrono>
#include <cstring>
#include <cassert>
#include <iostream>
#include <algorithm>

using namespace std::chrono;
using std::unique_ptr;
using std::make_unique;

heartbeater_impl::heartbeater_impl(environment &env)
    : lg(env.get<logger_factory>()->get_logger("heartbeater")),
      config(env.get<configuration>()),
      client(env.get<udp_factory>()->get_udp_client()),
      server(env.get<udp_factory>()->get_udp_server()),
      nodes_can_join(true), mem_list(env)
{
    our_id = 0;

    joined_group = false;
    if (config->is_first_node()) {
        int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        our_id = std::hash<std::string>()(config->get_hostname()) ^ std::hash<int>()(join_time);

        mem_list.add_member(config->get_hostname(), our_id);
        joined_ids.insert(our_id);
        joined_group = true;
    }
}

// Starts the heartbeater
void heartbeater_impl::start() {
    lg->info("Starting heartbeater");

    running = true;

    server_thread = make_unique<std::thread>([this] {server_thread_function();});
    server_thread->detach();

    client_thread = make_unique<std::thread>([this] {client_thread_function();});
    client_thread->detach();
}

// Stops the heartbeater synchronously
void heartbeater_impl::stop() {
    lg->info("Stopping heartbeater");

    server->stop_server();
    running = false;

    // Sleep for enough time for the threads to stop and then delete them
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

// Returns the list of members of the group that this node is aware of
std::vector<member> heartbeater_impl::get_members() {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);

    return mem_list.get_members();
}

// Gets the member object corresponding to the provided ID
member heartbeater_impl::get_member_by_id(uint32_t id) {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);

    return mem_list.get_member_by_id(id);
}

// Returns the list of members of the group that this node is aware of
member heartbeater_impl::get_successor() {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);

    return mem_list.get_successor(our_id);
}

// Client thread function
void heartbeater_impl::client_thread_function() {
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

            // Create the message to send out
            hb_message msg(our_id);
            msg.set_failed_nodes(failed_nodes);
            msg.set_left_nodes(left_nodes);
            msg.set_joined_nodes(joined_nodes);
            std::string msg_str = msg.serialize();
            assert(msg_str.length() > 0);

            // Send the message out to all the neighbors
            for (auto mem : mem_list.get_neighbors()) {
                client->send(mem.hostname, config->get_hb_port(), msg_str);
            }

            // Send the introduction messages
            if (nodes_can_join.load() && new_nodes_queue.size() > 0) {
                send_introducer_msg();
            }
        }

        // Sleep for heartbeat_interval milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval_ms));
    }
}

// Scans through neighbors and marks those with a heartbeat past the timeout as failed
void heartbeater_impl::check_for_failed_neighbors() {
    uint64_t current_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::vector<member> neighbors = mem_list.get_neighbors();

    for (auto mem : neighbors) {
        if (current_time > mem.last_heartbeat + timeout_interval_ms) {
            lg->info("Node at " + mem.hostname + " with id " + std::to_string(mem.id) + " timed out!");
            mem_list.remove_member(mem.id);

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

// Sends pending messages to newly joined nodes
void heartbeater_impl::send_introducer_msg() {
    std::vector<member> all_members = mem_list.get_members();
    hb_message intro_msg(our_id);
    intro_msg.set_joined_nodes(all_members);
    std::string msg_str = intro_msg.serialize();
    assert(msg_str.length() > 0);

    auto new_nodes = new_nodes_queue.pop();
    auto updated = new_nodes_queue.peek();

    for (auto node : new_nodes) {
        lg->debug("Sent introducer message to host at " + node.hostname + " with ID " + std::to_string(node.id));
        client->send(node.hostname, config->get_hb_port(), msg_str);

        // If this node isn't in new_nodes_queue anymore, we should now add it to joined_nodes_queue
        // so that it can be added to other nodes' membership lists
        if (std::find_if(updated.begin(), updated.end(), [=](member m) {return m.id == node.id;}) == updated.end()) {
            joined_nodes_queue.push(node, message_redundancy);
        }
    }
}

// Runs the provided function atomically with any functions that read or write to the membership list
void heartbeater_impl::run_atomically_with_mem_list(std::function<void()> fn) {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);
    fn();
}

// Initiates an async request to join the group by sending a message a node to let us in
void heartbeater_impl::join_group(std::string node) {
    if (config->is_first_node())
        return;

    lg->info("Requesting node at " + node + " to join group");
    joined_group = true;

    int join_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    our_id = std::hash<std::string>()(config->get_hostname()) ^ std::hash<int>()(join_time);

    member us;
    us.id = our_id;
    us.hostname = config->get_hostname();

    hb_message join_req(our_id);
    join_req.make_join_request(us);
    std::string msg_str = join_req.serialize();
    assert(msg_str.length() > 0);

    for (int i = 0; i < message_redundancy; i++) {
        client->send(node, config->get_hb_port(), msg_str);
    }
}

// Sends a message to peers stating that we are leaving
void heartbeater_impl::leave_group() {
    lg->info("Leaving the group");

    joined_group = false;
    left_nodes_queue.push(our_id, message_redundancy);
}

// Server thread function
void heartbeater_impl::server_thread_function() {
    lg->debug("Starting server thread");
    server->start_server(config->get_hb_port());

    int size;
    char buf[1024];

    // Code to listen for messages and handle them accordingly here
    while (true) {
        // If the server is not running, stop everything and exit
        if (!running.load()) {
            lg->debug("Exiting server thread");
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
                lg->debug("Received malformed heartbeat message!");
                continue;
            }

            // If we are in the group and the message is not from within the group, ignore it
            if (mem_list.get_member_by_id(our_id).id == our_id &&
                mem_list.get_member_by_id(msg.get_id()).id != msg.get_id())
            {
                // Allow the message to be processed if it's a join request
                if (msg.is_join_request()) {
                    if (!nodes_can_join.load()) {
                        lg->debug("Nodes cannot join because leader election is occurring!");
                        continue;
                    }
                } else {
                    lg->trace("Ignoring heartbeat message from " + std::to_string(msg.get_id()) + " because they are not in the group");
                    continue;
                }
            }

            if (msg.is_join_request()) {
                member m = msg.get_join_request();

                // Make sure this is a node we haven't seen yet
                if (joined_ids.find(m.id) == joined_ids.end()) {
                    joined_ids.insert(m.id);
                    mem_list.add_member(m.hostname, m.id);

                    lg->info("Received request to join group from (" + m.hostname + ", " + std::to_string(m.id) + ")");
                    new_nodes_queue.push(m, message_redundancy);

                    // Call the join handlers after adding this node
                    for (auto handler : on_join_handlers) {
                        handler(m);
                    }
                }
            }

            for (member m : msg.get_joined_nodes()) {
                assert(m.id != 0 && m.hostname != "");

                // Only add and propagate information about this join if we've never seen this node
                if (joined_ids.find(m.id) == joined_ids.end()) {
                    joined_ids.insert(m.id);
                    mem_list.add_member(m.hostname, m.id);

                    // Check if the newly joined member is us
                    if (m.id == our_id) {
                        lg->info("Successfully joined group");
                    }

                    joined_nodes_queue.push(m, message_redundancy);

                    for (auto handler : on_join_handlers) {
                        handler(m);
                    }
                }
            }

            for (uint32_t id : msg.get_left_nodes()) {
                // Only propagate this message if the member has not yet been removed
                member mem = mem_list.get_member_by_id(id);
                if (mem.id == id) {
                    mem_list.remove_member(id);
                    left_nodes_queue.push(id, message_redundancy);

                    for (auto handler : on_leave_handlers) {
                        handler(mem);
                    }
                }
            }

            for (uint32_t id : msg.get_failed_nodes()) {
                // Only propagate this message if the member has not yet been removed
                member mem = mem_list.get_member_by_id(id);
                if (mem.id == id) {
                    mem_list.remove_member(id);
                    failed_nodes_queue.push(id, message_redundancy);

                    for (auto handler : on_fail_handlers) {
                        handler(mem);
                    }
                }
            }

            mem_list.update_heartbeat(msg.get_id());
        }
    }
}

// Adds a handler to the list of handlers that will be called when a node fails
void heartbeater_impl::on_fail(std::function<void(member)> handler) {
    // Acquire mutex to prevent concurrent modification of vector
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);
    on_fail_handlers.push_back(handler);
}

// Adds a handler to the list of handlers that will be called when a node leaves
void heartbeater_impl::on_leave(std::function<void(member)> handler) {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);
    on_leave_handlers.push_back(handler);
}

// Adds a handler to the list of handlers that will be called when a node joins
void heartbeater_impl::on_join(std::function<void(member)> handler) {
    std::lock_guard<std::recursive_mutex> guard(member_list_mutex);
    on_join_handlers.push_back(handler);
}

// Register the service
register_auto<heartbeater, heartbeater_impl> register_heartbeater;
