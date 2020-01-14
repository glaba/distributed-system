#include "mock_udp.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <chrono>

using namespace std::chrono;
using std::queue;
using std::string;
using std::unique_ptr;
using std::make_unique;

auto mock_udp_factory::get_udp_client() -> unique_ptr<udp_client> {
    return get_udp_client(config->get_hostname());
}

auto mock_udp_factory::get_udp_server() -> unique_ptr<udp_server> {
    return get_udp_server(config->get_hostname());
}

auto mock_udp_factory::init_state() -> unique_ptr<service_state> {
    mock_udp_state *state = new mock_udp_state();
    state->coordinator = std::make_unique<mock_udp_coordinator>();
    return std::unique_ptr<service_state>(static_cast<service_state*>(state));
}

void mock_udp_factory::reinitialize(environment &env_) {
    env = &env_;
    config = env->get<configuration>();
}

auto mock_udp_factory::get_udp_client(string const& hostname) -> unique_ptr<udp_client> {
    mock_udp_coordinator *coordinator = unlocked<mock_udp_state>::dyn_cast(access_state())->coordinator.get();

    return make_unique<mock_udp_client>(hostname, show_packets, drop_probability,
        coordinator, env->get<logger_factory>()->get_logger("mock_udp_client"));
}

auto mock_udp_factory::get_udp_server(string const& hostname) -> unique_ptr<udp_server> {
    mock_udp_coordinator *coordinator = unlocked<mock_udp_state>::dyn_cast(access_state())->coordinator.get();

    return make_unique<mock_udp_server>(hostname, coordinator);
}

// Notifies the coordinator that the server is now started
void mock_udp_factory::mock_udp_coordinator::start_server(host const& h) {
    unlocked<coordinator_state> coord_state = coord_state_lock();

    if (coord_state->msg_queues.find(h) != coord_state->msg_queues.end()) {
        assert(false && "start_server has already been called");
    }

    coord_state->msg_queues[h] = queue<string>();
}

// Notify the coordinator that this thread is waiting for messages
// and will wake up when flag is set to true
void mock_udp_factory::mock_udp_coordinator::notify_waiting(host const& h, notify_state const& ns) {
    unlocked<coordinator_state> coord_state = coord_state_lock();

    if (coord_state->msg_queues.find(h) == coord_state->msg_queues.end()) {
        return;
    }

    assert(coord_state->cv_map.find(h) == coord_state->cv_map.end());

    if (coord_state->msg_queues[h].size() > 0) {
        std::lock_guard<std::recursive_mutex> guard(ns.m);
        ns.flag = true;
        ns.cv.notify_one();
    } else {
        coord_state->cv_map.emplace(h, ns);
    }
}

// Reads a packet (non-blocking) after notify_waiting was called and flag was set to true
auto mock_udp_factory::mock_udp_coordinator::recv(host const& h, char *buf, unsigned length) -> int {
    string msg;
    {
        unlocked<coordinator_state> coord_state = coord_state_lock();

        if (coord_state->msg_queues.find(h) == coord_state->msg_queues.end()) {
            return 0;
        }

        auto &messages = coord_state->msg_queues[h];
        if (messages.size() == 0) {
            return 0;
        }

        msg = std::move(messages.front());
        messages.pop();
    }

    unsigned i;
    for (i = 0; i < msg.length() && i < length; i++) {
        buf[i] = msg.at(i);
    }

    return static_cast<int>(i);
}

// Sends a packet to the specified destination
void mock_udp_factory::mock_udp_coordinator::send(host const& dest, char const* msg_buf, unsigned length) {
    assert(msg_buf != nullptr);
    string msg = string(msg_buf, length);

    unlocked<coordinator_state> coord_state = coord_state_lock();

    // If there is no server listening at dest, do nothing
    if (coord_state->msg_queues.find(dest) == coord_state->msg_queues.end()) {
        return;
    }

    coord_state->msg_queues[dest].push(std::move(msg));

    // If a dest is waiting for a message, notify it that a message is ready
    if (coord_state->cv_map.find(dest) != coord_state->cv_map.end()) {
        notify_state &ns = coord_state->cv_map.at(dest);

        std::lock_guard<std::recursive_mutex> guard(ns.m);
        ns.flag = true;
        ns.cv.notify_one();

        coord_state->cv_map.erase(dest);
    }
}

// Clears the message queue for this host and notifies with no message if recv is being called
void mock_udp_factory::mock_udp_coordinator::stop_server(host const& h) {
    unlocked<coordinator_state> coord_state = coord_state_lock();
    if (coord_state->msg_queues.find(h) == coord_state->msg_queues.end()) {
        assert(false && "Cannot call stop_server before start_server is called");
    }

    // Clear all data associated with the host
    coord_state->msg_queues.erase(h);
    coord_state->cv_map.erase(h);

    // The recv wrapper in mock_udp_server should automatically return at this point
}

// Sends a UDP packet to the specified destination
void mock_udp_factory::mock_udp_client::send(string const& dest, int port, string const& msg) {
    if (drop_probability == 0 || static_cast<double>(std::rand() % RAND_MAX) / RAND_MAX >= drop_probability) {
        if (show_packets) {
            string log_msg = "[Delivered on port " +
                std::to_string(port) + "] " +
                hostname + " -> " + dest + " - ";
            for (unsigned i = 0; i < msg.length(); i++) {
                log_msg += std::to_string(msg[i]) + " ";
            }
            lg->info(log_msg);
        }
        coordinator->send(host(dest, port), msg.c_str(), msg.length());
    }
    else if (show_packets) {
        string log_msg = "[Dropped on port " +
            std::to_string(port) + "] " +
            hostname + " -> " + dest + " - ";
        for (unsigned i = 0; i < msg.length(); i++) {
            log_msg += std::to_string(msg[i]) + " ";
        }
        lg->info(log_msg);
    }
}

// Starts the server on the machine with the given hostname on the given port
void mock_udp_factory::mock_udp_server::start_server(int port) {
    {
        unlocked<server_state> serv_state = serv_state_lock();
        serv_state->port = port;
    }
    coordinator->start_server(host(hostname, port));
}

// Stops the server, this function is not threadsafe and should only be called once
void mock_udp_factory::mock_udp_server::stop_server() {
    int port = serv_state_lock()->port;
    coordinator->stop_server(host(hostname, port));

    unlocked<server_state> serv_state = serv_state_lock();
    serv_state->port = 0;
    cv_msg.notify_one();
}

// Wrapper function around recv that handles errors
auto mock_udp_factory::mock_udp_server::recv(char *buf, unsigned length) -> int {
    bool flag = false;

    // Notify the coordinator that we are waiting for a message
    int port = serv_state_lock()->port;
    coordinator->notify_waiting(host(hostname, port), notify_state(serv_state_lock.unsafe_get_mutex(), cv_msg, flag));

    // Wait for a message to arrive or for the server to stop
    {
        unlocked<server_state> serv_state = serv_state_lock();
        cv_msg.wait(serv_state.unsafe_get_mutex(), [&] {
            return serv_state->port == 0 || flag;
        });

        if (serv_state->port == 0) {
            return 0;
        }
    }

    return coordinator->recv(host(hostname, port), buf, length);
}

register_test_service<udp_factory, mock_udp_factory> register_mock_udp_factory;
