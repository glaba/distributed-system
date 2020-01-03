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
    return std::unique_ptr<service_state>(state);
}

void mock_udp_factory::reinitialize(environment &env_) {
    env = &env_;
    config = env->get<configuration>();
}

auto mock_udp_factory::get_udp_client(string const& hostname) -> unique_ptr<udp_client> {
    mock_udp_coordinator *coordinator;
    access_state([&coordinator] (service_state *state) {
        coordinator = dynamic_cast<mock_udp_state*>(state)->coordinator.get();
    });

    return make_unique<mock_udp_client>(hostname, show_packets, drop_probability,
        coordinator, env->get<logger_factory>()->get_logger("mock_udp_client"));
}

auto mock_udp_factory::get_udp_server(string const& hostname) -> unique_ptr<udp_server> {
    mock_udp_coordinator *coordinator;
    access_state([&coordinator] (service_state *state) {
        coordinator = dynamic_cast<mock_udp_state*>(state)->coordinator.get();
    });

    return make_unique<mock_udp_server>(hostname, coordinator);
}

// Notifies the coordinator that the server is now started
void mock_udp_factory::mock_udp_port_coordinator::start_server(string const& hostname) {
    std::lock_guard<std::mutex> guard(msg_mutex);

    if (msg_queues.find(hostname) != msg_queues.end()) {
        assert(false && "start_server has already been called");
    }

    msg_queues[hostname] = std::queue<std::tuple<unique_ptr<char[]>, unsigned>>();
    notify_flags[hostname] = nullptr;
}

// Notify the coordinator that this thread is waiting for messages
// and will wake up when flag is set to true
void mock_udp_factory::mock_udp_port_coordinator::notify_waiting(string const& hostname, volatile bool *flag) {
    std::lock_guard<std::mutex> guard(msg_mutex);

    if (msg_queues.find(hostname) == msg_queues.end()) {
        assert(false && "notify_waiting should not be called before start_server");
    }

    assert(notify_flags[hostname] == nullptr);
    notify_flags[hostname] = flag;

    if (msg_queues[hostname].size() > 0) {
        *flag = true;
        notify_flags[hostname] = nullptr;
    }
}

// Reads a packet (non-blocking) after notify_waiting was called and flag was set to true
auto mock_udp_factory::mock_udp_port_coordinator::recv(string const& hostname, char *buf, unsigned length) -> int {
    std::lock_guard<std::mutex> guard(msg_mutex);

    if (msg_queues.find(hostname) == msg_queues.end()) {
        assert(false && "recv should not be called before start_server");
    }

    std::queue<std::tuple<unique_ptr<char[]>, unsigned>> &messages = msg_queues[hostname];
    if (messages.size() == 0) {
        return 0;
    }

    auto &&[msg_buf, actual_length] = messages.front();
    assert(msg_buf != nullptr);

    unsigned i;
    for (i = 0; i < actual_length && i < length; i++) {
        buf[i] = msg_buf[i];
    }

    messages.pop();

    return static_cast<int>(i);
}

// Sends a packet to the specified destination
void mock_udp_factory::mock_udp_port_coordinator::send(string const& dest, const char *msg, unsigned length) {
    std::lock_guard<std::mutex> guard(msg_mutex);
    assert(msg != nullptr);

    // If there is no server listening at dest, do nothing
    if (msg_queues.find(dest) == msg_queues.end()) {
        return;
    }

    unique_ptr<char[]> msg_buf = make_unique<char[]>(length);
    for (unsigned i = 0; i < length; i++) {
        msg_buf[i] = msg[i];
    }

    msg_queues[dest].push({std::move(msg_buf), length});

    if (notify_flags[dest] != nullptr) {
        *notify_flags[dest] = true;
        notify_flags[dest] = nullptr;
    }
}

// Clears the message queue for this host and notifies with no message if recv is being called
void mock_udp_factory::mock_udp_port_coordinator::stop_server(string const& hostname) {
    std::lock_guard<std::mutex> guard(msg_mutex);

    if (msg_queues.find(hostname) == msg_queues.end()) {
        assert(false && "Cannot call stop_server before start_server is called");
    }

    // Clear the queue, which should automatically free all memory
    msg_queues.erase(hostname);

    // The recv wrapper in mock_udp_server should automatically return at this point
}

// A wrapper that multiplexes the start_server call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::start_server(string const& hostname, int port) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        coordinators[port] = std::make_unique<mock_udp_port_coordinator>();
    }

    coordinators[port]->start_server(hostname);
}

// A wrapper that multiplexes the notify_waiting call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::notify_waiting(string const& hostname, int port, volatile bool *flag) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        assert(false && "notify_waiting should not be called before start_server");
    }

    coordinators[port]->notify_waiting(hostname, flag);
}

// A wrapper that multiplexes the recv call to the correct mock_udp_port_coordinator
auto mock_udp_factory::mock_udp_coordinator::recv(string const& hostname, int port, char *buf, unsigned length) -> int {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        assert(false && "recv should not be called before start_server");
    }

    return coordinators[port]->recv(hostname, buf, length);
}

// A wrapper that multiplexes the send call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::send(string const& dest, int port, const char *msg, unsigned length) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        // There is no one listening
        return;
    }

    coordinators[port]->send(dest, msg, length);
}

// A wrapper that multiplexes the stop_server call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::stop_server(string const& hostname, int port) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        assert(false && "Cannot call stop_server before start_server is called");
    }

    coordinators[port]->stop_server(hostname);
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
    std::lock_guard<std::mutex> guard(port_mutex);
    port = port_;
    coordinator->start_server(hostname, port);
}

// Stops the server
void mock_udp_factory::mock_udp_server::stop_server() {
    std::lock_guard<std::mutex> guard(port_mutex);
    coordinator->stop_server(hostname, port);
    port = 0;
}

// Wrapper function around recvfrom that handles errors
auto mock_udp_factory::mock_udp_server::recv(char *buf, unsigned length) -> int {
    volatile bool flag = false;

    // Make sure the server is running and notify the coordinator if so
    {
        std::lock_guard<std::mutex> guard(port_mutex);
        if (port == 0) {
            return 0;
        } else {
            coordinator->notify_waiting(hostname, port, &flag);
        }
    }

    // Wait for a message to arrive while the server is still up
    while (!flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::lock_guard<std::mutex> guard(port_mutex);
        if (port == 0) {
            return 0;
        }
    }

    std::lock_guard<std::mutex> guard(port_mutex);
    if (port == 0) {
        return 0;
    } else {
        return coordinator->recv(hostname, port, buf, length);
    }
}

register_test_service<udp_factory, mock_udp_factory> register_mock_udp_factory;
