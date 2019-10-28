#pragma once

#include "utils.h"

#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <tuple>

using std::string;

// Coordinator class which passes mock UDP messages around
// Should not be instantiated
class mock_udp_coordinator {
public:
    // Notify the coordinator that this thread is waiting for messages
    // and will wake up when flag is set to true
    void notify_waiting(string hostname, volatile bool *flag);
    // Reads a packet (non-blocking) after notify_waiting was called and flag was set to true
    int recv(string hostname, char *buf, unsigned length);
    // Clears the message queue for this host and notifies with no message if recv is being called
    void stop_server(string hostname);
    // Sends a packet to the specified destination
    void send(string dest, char *msg, unsigned length);
private:
    // Lock access to both maps
    std::mutex msg_mutex;
    // A map from hostname of receiving machine to a queue of (char *msg, unsigned length)
    std::unordered_map<std::string, std::queue<std::tuple<char*, unsigned>>> msg_queues;
    // A map of hostnames that are waiting for a message to arrive to a pointer to the flag that should be set to wake them
    std::unordered_map<std::string, volatile bool*> notify_flag;
};

// Mock UDP client service, should not be instantiated but obtained from the factory
class mock_udp_client_svc : public udp_client_svc {
public:
    mock_udp_client_svc(string hostname_, bool show_packets_, mock_udp_coordinator *coordinator_)
        : udp_client_svc(nullptr), show_packets(show_packets_), drop_probability(0.0), hostname(hostname_), coordinator(coordinator_) {}
    ~mock_udp_client_svc() {}

    // Sends a UDP packet to the specified destination
    void send(string host, string port, char *msg, unsigned length);

    // Set the probability that a message will be dropped
    void set_drop_probability(double p) {
        drop_probability = p;
    }
private:
    bool show_packets;
    double drop_probability;
    string hostname;
    mock_udp_coordinator *coordinator;
};

// Mock UDP server service, should not be instantiated but obtained from the factory
class mock_udp_server_svc : public udp_server_svc {
public:
    mock_udp_server_svc(string hostname_, mock_udp_coordinator *coordinator_) 
        : udp_server_svc(nullptr), hostname(hostname_), coordinator(coordinator_), stopped(false) {}
    ~mock_udp_server_svc() {}

    // Starts the server on the machine with the given hostname on the given port
    void start_server(int port);
    // Stops the server
    void stop_server();
    // Wrapper function around recvfrom that handles errors
    int recv(char *buf, unsigned length);
private:
    string hostname;
    mock_udp_coordinator *coordinator;
    bool stopped;
};

// Factory which produces mock UDP clients and servers
// Only this class should be instantiated
class mock_udp_factory {
public:
    mock_udp_factory() {
        coordinator = new mock_udp_coordinator();
    }

    ~mock_udp_factory() {
        delete coordinator;
    }

    mock_udp_client_svc *get_mock_udp_client(string hostname, bool show_packets);
    mock_udp_server_svc *get_mock_udp_server(string hostname);
private:
    mock_udp_coordinator *coordinator;
};
