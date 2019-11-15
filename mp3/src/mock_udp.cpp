#include "mock_udp.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <cassert>
#include <cstdlib>

using std::unique_ptr;
using std::make_unique;

std::unique_ptr<udp_client_intf> mock_udp_factory::get_mock_udp_client(string hostname, bool show_packets, double drop_probability) {
    return std::make_unique<mock_udp_client>(hostname, show_packets, drop_probability, coordinator.get());
}

std::unique_ptr<udp_server_intf> mock_udp_factory::get_mock_udp_server(string hostname) {
    return std::make_unique<mock_udp_server>(hostname, coordinator.get());
}

// Notifies the coordinator that the server is now started
void mock_udp_factory::mock_udp_port_coordinator::start_server(string hostname) {
    std::lock_guard<std::mutex> guard(msg_mutex);

    if (msg_queues.find(hostname) != msg_queues.end()) {
        assert(false && "start_server has already been called");
    }

    msg_queues[hostname] = std::queue<std::tuple<unique_ptr<char[]>, unsigned>>();
    notify_flags[hostname] = nullptr;
}

// Notify the coordinator that this thread is waiting for messages
// and will wake up when flag is set to true
void mock_udp_factory::mock_udp_port_coordinator::notify_waiting(string hostname, volatile bool *flag) {
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
int mock_udp_factory::mock_udp_port_coordinator::recv(string hostname, char *buf, unsigned length) {
    std::lock_guard<std::mutex> guard(msg_mutex);

    if (msg_queues.find(hostname) == msg_queues.end()) {
        assert(false && "recv should not be called before start_server");
    }

    std::queue<std::tuple<unique_ptr<char[]>, unsigned>> &messages = msg_queues[hostname];
    if (messages.size() == 0) {
        return 0;
    }

    std::tuple<unique_ptr<char[]>, unsigned> &msg = messages.front();

    char *msg_buf = std::get<0>(msg).get();
    unsigned actual_length = std::get<1>(msg);
    assert(msg_buf != nullptr);

    unsigned i;
    for (i = 0; i < actual_length && i < length; i++) {
        buf[i] = msg_buf[i];
    }

    messages.pop();

    return static_cast<int>(i);
}

// Sends a packet to the specified destination
void mock_udp_factory::mock_udp_port_coordinator::send(string dest, char *msg, unsigned length) {
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

    msg_queues[dest].push(std::make_tuple(std::move(msg_buf), length));

    if (notify_flags[dest] != nullptr) {
        *notify_flags[dest] = true;
        notify_flags[dest] = nullptr;
    }
}

// Clears the message queue for this host and notifies with no message if recv is being called
void mock_udp_factory::mock_udp_port_coordinator::stop_server(string hostname) {
    std::lock_guard<std::mutex> guard(msg_mutex);

    if (msg_queues.find(hostname) == msg_queues.end()) {
        assert(false && "Cannot call stop_server before start_server is called");
    }

    // Clear the queue, which should automatically free all memory
    msg_queues.erase(hostname);

    // The recv wrapper in mock_udp_server should automatically return at this point
}

// A wrapper that multiplexes the start_server call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::start_server(string hostname, int port) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        coordinators[port] = std::make_unique<mock_udp_port_coordinator>();
    }

    coordinators[port]->start_server(hostname);
}

// A wrapper that multiplexes the notify_waiting call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::notify_waiting(string hostname, int port, volatile bool *flag) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        assert(false && "notify_waiting should not be called before start_server");
    }

    coordinators[port]->notify_waiting(hostname, flag);
}

// A wrapper that multiplexes the recv call to the correct mock_udp_port_coordinator
int mock_udp_factory::mock_udp_coordinator::recv(string hostname, int port, char *buf, unsigned length) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        assert(false && "recv should not be called before start_server");
    }

    return coordinators[port]->recv(hostname, buf, length);
}

// A wrapper that multiplexes the send call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::send(string dest, int port, char *msg, unsigned length) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        // There is no one listening
        return;
    }

    coordinators[port]->send(dest, msg, length);
}

// A wrapper that multiplexes the stop_server call to the correct mock_udp_port_coordinator
void mock_udp_factory::mock_udp_coordinator::stop_server(string hostname, int port) {
    std::lock_guard<std::mutex> guard(coordinators_mutex);
    if (coordinators.find(port) == coordinators.end()) {
        assert(false && "Cannot call stop_server before start_server is called");
    }

    coordinators[port]->stop_server(hostname);
}

// Sends a UDP packet to the specified destination
void mock_udp_factory::mock_udp_client::send(string dest, int port, char *msg, unsigned length) {
    if (static_cast<double>(std::rand() % RAND_MAX) / RAND_MAX >= drop_probability) {
        if (show_packets) {
            std::string log_msg = "[Delivered " +
                std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()) + "] " +
                hostname + " -> " + dest + " - ";
            for (unsigned i = 0; i < length; i++) {
                log_msg += std::to_string(msg[i]) + " ";
            }
            lg->info(log_msg);
        }
        coordinator->send(dest, port, msg, length);
    }
    else if (show_packets) {
        std::string log_msg = "[Dropped " +
            std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()) + "] " +
            hostname + " -> " + dest + " - ";
        for (unsigned i = 0; i < length; i++) {
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
int mock_udp_factory::mock_udp_server::recv(char *buf, unsigned length) {
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
