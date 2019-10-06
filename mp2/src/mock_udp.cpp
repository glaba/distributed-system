#include "mock_udp.h"

udp_client_svc *mock_udp_factory::get_mock_udp_client(string hostname) {
    return new mock_udp_client_svc(hostname, coordinator);
}

udp_server_svc *mock_udp_factory::get_mock_udp_server(string hostname) {
    return new mock_udp_server_svc(hostname, coordinator);
}

// Notify the coordinator that this thread is waiting for messages
// and will wake up when flag is set to true
void mock_udp_coordinator::notify_waiting(string hostname, volatile bool *flag) {
    std::lock_guard<std::mutex> guard(msg_mutex);
    notify_flag[hostname] = flag;

    if (msg_queues[hostname].size() > 0) {
        *flag = true;
        notify_flag[hostname] = nullptr;
    }
}

// Reads a packet (non-blocking) after notify_waiting was called and flag was set to true
int mock_udp_coordinator::recv(string hostname, char *buf, unsigned length) {
    std::lock_guard<std::mutex> guard(msg_mutex);
    std::queue<std::tuple<char*, unsigned>> &messages = msg_queues[hostname];

    if (messages.size() == 0) {
        return 0;
    }

    std::tuple<char*, unsigned> msg = messages.front();
    messages.pop();

    char *msg_buf = std::get<0>(msg);
    unsigned actual_length = std::get<1>(msg);

    unsigned i;
    for (i = 0; i < actual_length && i < length; i++) {
        buf[i] = msg_buf[i];
    }

    delete msg_buf;

    return static_cast<int>(i);
}

// Sends a packet to the specified destination
void mock_udp_coordinator::send(string dest, char *msg, unsigned length) {
    std::lock_guard<std::mutex> guard(msg_mutex);

    char *msg_buf = new char[length];
    for (unsigned i = 0; i < length; i++) {
        msg_buf[i] = msg[i];
    }

    msg_queues[dest].push(std::make_tuple(msg_buf, length));

    if (notify_flag[dest] != nullptr) {
        *notify_flag[dest] = true;
    }
}

// Clears the message queue for this host and notifies with no message if recv is being called
void mock_udp_coordinator::stop_server(string hostname) {
    std::lock_guard<std::mutex> guard(msg_mutex);

    std::queue<std::tuple<char*, unsigned>> &messages = msg_queues[hostname];

    // Clear the queue and free allocated memory
    while (messages.size() > 0) {
        delete std::get<0>(messages.front());
        messages.pop();
    }

    // Notify if recv is being called
    if (notify_flag[hostname] != nullptr) {
        *notify_flag[hostname] = true;
    }
}

void mock_udp_client_svc::send(string dest, string port, char *msg, unsigned length) {
    coordinator->send(dest, msg, length);
}

// Starts the server on the machine with the given hostname on the given port
void mock_udp_server_svc::start_server(int port) {
    // Do nothing, we ignore ports for the mocked UDP service
}

// Stops the server
void mock_udp_server_svc::stop_server() {
    stopped = false;
    coordinator->stop_server(hostname);
}

// Wrapper function around recvfrom that handles errors
int mock_udp_server_svc::recv(char *buf, unsigned length) {
    volatile bool flag = false;
    
    coordinator->notify_waiting(hostname, &flag);
    while (!flag && !stopped); // Wait for a message to arrive while the server is still up

    return coordinator->recv(hostname, buf, length);
}
