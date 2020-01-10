#include "mock_udp.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <chrono>

using namespace std::chrono;
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
void mock_udp_factory::mock_udp_port_coordinator::start_server(string const& hostname) {
    unlocked<coordinator_state> coord_state = coord_state_lock();

    if (coord_state->msg_queues.find(hostname) != coord_state->msg_queues.end()) {
        assert(false && "start_server has already been called");
    }

    coord_state->msg_queues[hostname] = std::queue<std::tuple<unique_ptr<char[]>, unsigned>>();
    coord_state->notify_flags[hostname] = nullptr;
}

// Notify the coordinator that this thread is waiting for messages
// and will wake up when flag is set to true
void mock_udp_factory::mock_udp_port_coordinator::notify_waiting(string const& hostname, volatile bool *flag) {
    unlocked<coordinator_state> coord_state = coord_state_lock();

    if (coord_state->msg_queues.find(hostname) == coord_state->msg_queues.end()) {
        assert(false && "notify_waiting should not be called before start_server");
    }

    assert(coord_state->notify_flags[hostname] == nullptr);
    coord_state->notify_flags[hostname] = flag;

    if (coord_state->msg_queues[hostname].size() > 0) {
        *flag = true;
        coord_state->notify_flags[hostname] = nullptr;
    }
}

// Reads a packet (non-blocking) after notify_waiting was called and flag was set to true
auto mock_udp_factory::mock_udp_port_coordinator::recv(string const& hostname, char *buf, unsigned length) -> int {
    unique_ptr<char[]> msg_buf;
    unsigned actual_length;

    {
        unlocked<coordinator_state> coord_state = coord_state_lock();

        if (coord_state->msg_queues.find(hostname) == coord_state->msg_queues.end()) {
            assert(false && "recv should not be called before start_server");
        }

        auto &messages = coord_state->msg_queues[hostname];
        if (messages.size() == 0) {
            return 0;
        }

        std::tie(msg_buf, actual_length) = std::move(messages.front());
        messages.pop();
    }

    assert(msg_buf != nullptr);

    unsigned i;
    for (i = 0; i < actual_length && i < length; i++) {
        buf[i] = msg_buf[i];
    }

    return static_cast<int>(i);
}

// Sends a packet to the specified destination
void mock_udp_factory::mock_udp_port_coordinator::send(string const& dest, const char *msg, unsigned length) {
    assert(msg != nullptr);

    unique_ptr<char[]> msg_buf = make_unique<char[]>(length);
    for (unsigned i = 0; i < length; i++) {
        msg_buf[i] = msg[i];
    }

    unlocked<coordinator_state> coord_state = coord_state_lock();

    // If there is no server listening at dest, do nothing
    if (coord_state->msg_queues.find(dest) == coord_state->msg_queues.end()) {
        return;
    }

    coord_state->msg_queues[dest].push({std::move(msg_buf), length});

    if (coord_state->notify_flags[dest] != nullptr) {
        *coord_state->notify_flags[dest] = true;
        coord_state->notify_flags[dest] = nullptr;
    }
}

// Clears the message queue for this host and notifies with no message if recv is being called
void mock_udp_factory::mock_udp_port_coordinator::stop_server(string const& hostname) {
    unlocked<coordinator_state> coord_state = coord_state_lock();
    if (coord_state->msg_queues.find(hostname) == coord_state->msg_queues.end()) {
        assert(false && "Cannot call stop_server before start_server is called");
    }

    // Clear the queue, which should automatically free all memory
    coord_state->msg_queues.erase(hostname);

    // The recv wrapper in mock_udp_server should automatically return at this point
}

// A wrapper that multiplexes the start_server call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::start_server(string const& hostname, int port) {
    unlocked<coordinator_state> coord_state = coord_state_lock();
    if (coord_state->coordinators.find(port) == coord_state->coordinators.end()) {
        coord_state->coordinators[port] = std::make_unique<mock_udp_port_coordinator>();
    }

    coord_state->coordinators[port]->start_server(hostname);
}

// A wrapper that multiplexes the notify_waiting call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::notify_waiting(string const& hostname, int port, volatile bool *flag) {
    unlocked<coordinator_state> coord_state = coord_state_lock();
    if (coord_state->coordinators.find(port) == coord_state->coordinators.end()) {
        assert(false && "notify_waiting should not be called before start_server");
    }

    coord_state->coordinators[port]->notify_waiting(hostname, flag);
}

// A wrapper that multiplexes the recv call to the correct mock_udp_port_coordinator
auto mock_udp_factory::mock_udp_coordinator::recv(string const& hostname, int port, char *buf, unsigned length) -> int {
    unlocked<coordinator_state> coord_state = coord_state_lock();
    if (coord_state->coordinators.find(port) == coord_state->coordinators.end()) {
        assert(false && "recv should not be called before start_server");
    }

    return coord_state->coordinators[port]->recv(hostname, buf, length);
}

// A wrapper that multiplexes the send call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::send(string const& dest, int port, const char *msg, unsigned length) {
    unlocked<coordinator_state> coord_state = coord_state_lock();
    if (coord_state->coordinators.find(port) == coord_state->coordinators.end()) {
        // There is no one listening
        return;
    }

    coord_state->coordinators[port]->send(dest, msg, length);
}

// A wrapper that multiplexes the stop_server call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::stop_server(string const& hostname, int port) {
    unlocked<coordinator_state> coord_state = coord_state_lock();
    if (coord_state->coordinators.find(port) == coord_state->coordinators.end()) {
        assert(false && "Cannot call stop_server before start_server is called");
    }

    coord_state->coordinators[port]->stop_server(hostname);
}

// Sends a UDP packet to the specified destination
void mock_udp_factory::mock_udp_client::send(string const& dest, int port, string const& msg) {
    if (static_cast<double>(std::rand() % RAND_MAX) / RAND_MAX >= drop_probability) {
        if (show_packets) {
            string log_msg = "[Delivered on port " +
                std::to_string(port) + "] " +
                hostname + " -> " + dest + " - ";
            for (unsigned i = 0; i < msg.length(); i++) {
                log_msg += std::to_string(msg[i]) + " ";
            }
            lg->info(log_msg);
        }
        coordinator->send(dest, port, msg.c_str(), msg.length());
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
void mock_udp_factory::mock_udp_server::start_server(int port_) {
    port = port_;
    coordinator->start_server(hostname, port.load());
}

// Stops the server
void mock_udp_factory::mock_udp_server::stop_server() {
    coordinator->stop_server(hostname, port.load());
    port = 0;
}

// Wrapper function around recvfrom that handles errors
auto mock_udp_factory::mock_udp_server::recv(char *buf, unsigned length) -> int {
    volatile bool flag = false;

    // Make sure the server is running and notify the coordinator if so
    {
        if (port.load() == 0) {
            return 0;
        } else {
            coordinator->notify_waiting(hostname, port.load(), &flag);
        }
    }

    // Wait for a message to arrive while the server is still up
    while (!flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (port.load() == 0) {
            return 0;
        }
    }

    if (port.load() == 0) {
        return 0;
    } else {
        return coordinator->recv(hostname, port.load(), buf, length);
    }
}

register_test_service<udp_factory, mock_udp_factory> register_mock_udp_factory;
