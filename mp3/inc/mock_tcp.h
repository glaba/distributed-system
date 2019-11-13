#pragma once

#include "tcp.h"
#include "mock_udp.h"

#include <string>
#include <memory>
#include <queue>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <tuple>
#include <thread>

// Factory which produces mock TCP clients and servers
// Only this class should be instantiated
class mock_tcp_factory {
public:
    mock_tcp_factory() : factory(std::make_unique<mock_udp_factory>()) {}
    std::unique_ptr<tcp_client_intf> get_mock_tcp_client(string hostname, bool show_packets);
    std::unique_ptr<tcp_server_intf> get_mock_tcp_server(string hostname, bool show_packets);
private:
    // Initiate messages are of the format <magic byte><ID><hostname>
    static const char initiate_magic_byte = 0x0;
    // Regular messages are of the format <magic byte><ID><message>
    static const char msg_magic_byte = 0x1;
    // Accept messages are of the format <magic byte><ID>
    static const char accept_magic_byte = 0x2;
    // Closed connection messages are of the format <magic byte><ID>
    static const char close_magic_byte = 0x3;

    // All messages will be prefixed with 5 bytes for additional socket information
    // The first byte will be a magic byte indicating the type of information being sent
    // The next 4 bytes will be a unique ID identifying the host

    // Mock TCP server, which is just a wrapper around a no-failure mock UDP server
    class mock_tcp_server : public tcp_server_intf {
    public:
        mock_tcp_server(mock_udp_factory *factory_, string hostname_, bool show_packets_)
            : factory(factory_), hostname(hostname_), id(std::hash<std::string>()(hostname_)), show_packets(show_packets_) {}
        ~mock_tcp_server();

        void setup_server(int port_);
        void stop_server();
        int accept_connection();
        // Closes the connection with the client at the given FD
        // There is a race condition between calling this function and the client receiving messages sent just before!
        // This race condition technically also exists with TCP although it is unlikely -- here it is likely!
        void close_connection(int client_socket);
        std::string read_from_client(int client_fd);
        ssize_t write_to_client(int client_fd, std::string data);
    private:
        // Deletes all information stored about the specified client
        void delete_connection(int client_fd);

        mock_udp_factory *factory;
        std::unique_ptr<udp_server_intf> server;
        std::unique_ptr<udp_client_intf> client;
        string hostname;
        uint32_t id;
        int port;
        bool show_packets;

        // Thread which reads UDP messages and pushes them into the correct queue
        std::thread msg_thread;
        std::atomic<bool> running;

        // Queues for incoming connections as well as regular messages and mutex protecting them
        std::recursive_mutex msg_mutex;
        // A queue of tuples of the form (client id, client hostname)
        std::queue<std::tuple<uint32_t, std::string>> incoming_connections;
        std::unordered_map<uint32_t, std::queue<std::string>> msg_queues;

        // A map of client IDs to client hostnames
        std::unordered_map<uint32_t, std::string> client_hostnames;
    };

    // Mock TCP client, which is just a wrapper around a no-failure mock UDP client
    class mock_tcp_client : public tcp_client_intf {
    public:
        mock_tcp_client(mock_udp_factory *factory_, string hostname_, bool show_packets_)
            : factory(factory_), hostname(hostname_), id(std::hash<std::string>()(hostname_)), show_packets(show_packets_) {}
        ~mock_tcp_client();

        int setup_connection(std::string host, int port_);
        std::string read_from_server(int socket);
        ssize_t write_to_server(int socket, std::string data);
        void close_connection(int socket);
    private:
        // Deletes all information stored about the specified socket
        void delete_connection(int socket);

        mock_udp_factory *factory;
        std::unordered_map<uint32_t, std::unique_ptr<udp_server_intf>> servers;
        std::unordered_map<uint32_t, std::unique_ptr<udp_client_intf>> clients;
        string hostname;
        uint32_t id;
        int port;
        bool show_packets;

        // Threads which read UDP messages per server (indexed by server ID) and push them into the message queue
        std::unordered_map<uint32_t, std::thread> msg_threads;
        std::unordered_map<uint32_t, std::atomic<bool>> running;

        // Queues for messages and mutex protecting them
        std::recursive_mutex msg_mutex;
        std::unordered_map<uint32_t, std::queue<std::string>> msg_queues;

        // Indicator variable per socket which indicates whether or not we are currently reading from that socket
        std::unordered_map<uint32_t, bool> is_reading;

        // A map of server IDs to server hostnames
        std::unordered_map<uint32_t, std::string> server_hostnames;
    };

    std::unique_ptr<mock_udp_factory> factory;
};

