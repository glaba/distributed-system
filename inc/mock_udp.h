#pragma once

#include "udp.h"
#include "logging.h"
#include "configuration.h"
#include "environment.h"
#include "service.h"
#include "locking.h"

#include <string>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <memory>
#include <mutex>
#include <condition_variable>

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

    struct host {
        host(std::string const& hostname_, int port_) : hostname(hostname_), port(port_) {}
        auto operator==(host const& h) const -> bool {
            return hostname == h.hostname && port == h.port;
        }
        std::string hostname;
        int port;
    };

    struct host_hash {
        inline auto operator()(host const& v) const -> size_t {
            return (static_cast<size_t>(static_cast<uint32_t>(std::hash<std::string>()(v.hostname))) << 32) +
                    static_cast<uint32_t>(v.port);
        }
    };

    struct notify_state {
        notify_state(std::recursive_mutex &m_, std::condition_variable_any &cv_, bool &flag_)
            : m(m_), cv(cv_), flag(flag_) {}
        std::recursive_mutex &m;
        std::condition_variable_any &cv;
        bool &flag;
    };

    // Coordinator class which passes mock UDP messages around
    class mock_udp_coordinator {
    public:
        // Notifies the coordinator that the server is now started
        void start_server(host const& h);
        // Notify the coordinator that a thread is waiting for messages using the provided notify_state
        void notify_waiting(host const& h, notify_state const& ns);
        // Reads a packet (non-blocking) after notify_waiting was called and flag was set to true
        auto recv(host const& h, char *buf, unsigned length) -> int;
        // Sends a packet to the specified destination
        void send(host const& dest, char const* msg_buf, unsigned length);
        // Clears the message queue for this host and notifies with no message if recv is being called
        void stop_server(host const& h);
    private:
        struct coordinator_state {
            // A map from host of receiving machine to a queue of messages
            std::unordered_map<host, std::queue<std::string>, host_hash> msg_queues;
            // A map from host waiting for a message to the condition variable / mutex pair it is waiting on
            std::unordered_map<host, notify_state, host_hash> cv_map;
        };
        locked<coordinator_state> coord_state_lock;
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
            : hostname(hostname_), coordinator(coordinator_) {}
        ~mock_udp_server() {}

        // Starts the server on the machine with the given hostname on the given port
        void start_server(int port_);
        // Stops the server
        void stop_server();
        // Wrapper function around recvfrom that handles errors
        auto recv(char *buf, unsigned length) -> int;
    private:
        struct server_state {
            int port = 0;
        };
        locked<server_state> serv_state_lock;
        std::condition_variable_any cv_msg;

        std::string hostname;
        int port;
        mock_udp_coordinator *coordinator;
    };

    environment *env;
    configuration *config;

    bool show_packets = false;
    double drop_probability = 0.0;
};
