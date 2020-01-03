#pragma once

#include "udp.h"
#include "logging.h"
#include "configuration.h"
#include "environment.h"
#include "service.h"

#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <tuple>
#include <memory>

// Factory which produces mock UDP clients and servers
class mock_udp_factory : public udp_factory, public service_impl<mock_udp_factory> {
public:
    mock_udp_factory(environment &env_)
        : env(&env_),
          config(env->get<configuration>()) {}

    auto get_udp_client() -> std::unique_ptr<udp_client>;
    auto get_udp_server() -> std::unique_ptr<udp_server>;

    auto init_state() -> std::unique_ptr<service_state>;

    // Tests can call this method directly after safely casting udp_factory to mock_udp_factory
    void configure(bool show_packets_, double drop_probability_) {
        show_packets = show_packets_;
        drop_probability = drop_probability_;
    }

private:

    // Private methods that only mock_tcp_factory should use
    friend class mock_tcp_factory;
    void reinitialize(environment &env_);
    auto get_udp_client(std::string const& hostname) -> std::unique_ptr<udp_client>;
    auto get_udp_server(std::string const& hostname) -> std::unique_ptr<udp_server>;

    // Coordinator class which passes mock UDP messages around for one port
    class mock_udp_port_coordinator {
    public:
        // Notifies the coordinator that the server is now started
        void start_server(std::string const& hostname);
        // Notify the coordinator that this thread is waiting for messages
        // and will wake up when flag is set to true
        void notify_waiting(std::string const& hostname, volatile bool *flag);
        // Reads a packet (non-blocking) after notify_waiting was called and flag was set to true
        auto recv(std::string const& hostname, char *buf, unsigned length) -> int;
        // Sends a packet to the specified destination
        void send(std::string const& dest, const char *msg, unsigned length);
        // Clears the message queue for this host and notifies with no message if recv is being called
        void stop_server(std::string const& hostname);
    private:
        // Lock access to both maps
        std::mutex msg_mutex;
        // A map from hostname of receiving machine to a queue of (char *msg, unsigned length)
        std::unordered_map<std::string, std::queue<std::tuple<std::unique_ptr<char[]>, unsigned>>> msg_queues;
        // A map from hostname waiting for a message to arrive to a pointer to the flag that should be set to wake them
        std::unordered_map<std::string, volatile bool*> notify_flags;
    };

    // Coordinator class which passes mock UDP messages for any number of ports
    class mock_udp_coordinator {
    public:
        // All these functions essentially just multiplex to the correct mock_udp_port_coordinator
        void start_server(std::string const& hostname, int port);
        void notify_waiting(std::string const& hostname, int port, volatile bool *flag);
        auto recv(std::string const& hostname, int port, char *buf, unsigned length) -> int;
        void send(std::string const& dest, int port, const char *msg, unsigned length);
        void stop_server(std::string const& hostname, int port);
    private:
        std::mutex coordinators_mutex;
        std::unordered_map<int, std::unique_ptr<mock_udp_port_coordinator>> coordinators;
    };

    class mock_udp_state : public service_state {
    public:
        std::unique_ptr<mock_udp_coordinator> coordinator;
    };

    class mock_udp_client : public udp_client {
    public:
        mock_udp_client(std::string const& hostname_, bool show_packets_, double drop_probability_, mock_udp_coordinator *coordinator_, std::unique_ptr<logger> lg_)
            : show_packets(show_packets_), drop_probability(drop_probability_), hostname(hostname_),
              coordinator(coordinator_), lg(std::move(lg_)) {}

        // Sends a UDP packet to the specified destination
        void send(std::string const& host, int port, std::string const& msg);
    private:
        bool show_packets;
        double drop_probability;
        std::string hostname;
        mock_udp_coordinator *coordinator;
        std::unique_ptr<logger> lg;
    };

    class mock_udp_server : public udp_server {
    public:
        mock_udp_server(std::string const& hostname_, mock_udp_coordinator *coordinator_)
            : hostname(hostname_), port(0), coordinator(coordinator_) {}
        ~mock_udp_server() {}

        // Starts the server on the machine with the given hostname on the given port
        void start_server(int port_);
        // Stops the server
        void stop_server();
        // Wrapper function around recvfrom that handles errors
        auto recv(char *buf, unsigned length) -> int;
    private:
        std::string hostname;
        int port;
        std::mutex port_mutex;
        mock_udp_coordinator *coordinator;
    };

    environment *env;
    configuration *config;

    bool show_packets = false;
    double drop_probability = 0.0;
};
