// #pragma once

// #include "tcp.h"
// #include "mock_udp.h"

// #include <unordered_map>
// #include <string>

// // Factory which produces mock TCP clients and servers
// // Only this class should be instantiated
// class mock_tcp_factory {
// public:
//     mock_tcp_factory() {
//         factory = new mock_udp_factory();
//     }

//     ~mock_tcp_factory() {
//         delete factory;
//     }

//     mock_tcp_client *get_mock_tcp_client(string hostname, bool show_packets);
//     mock_tcp_server *get_mock_tcp_server(string hostname);
// private:
//     // Mock TCP server, which is just a wrapper around a no-failure mock UDP server
//     class mock_tcp_server : public tcp_server_intf {
//     public:
//         mock_tcp_server(udp_server_intf *factory_, string hostname_, bool show_packets_)
//             : factory(factory_), hostname(hostname_), show_packets(show_packets_) {}

//         void setup_server(int port);
//         void tear_down_server();
//         int accept_connection();
//         void close_connection(int client_socket);
//         ssize_t read_from_client(int client, char *buffer, size_t count);
//         ssize_t write_to_client(int client, const char *buffer, size_t count);
//     private:
//         mock_udp_factory *factory;
//         string hostname;
//         bool show_packets;
//     };

//     // Mock TCP client, which is just a wrapper around a no-failure mock UDP client
//     class mock_tcp_client : public tcp_client_intf {
//     public:
//         mock_tcp_client(mock_udp_factory *factory_, string hostname_)
//             : factory(factory_), hostname(hsotname_) {}

//         int setup_connection(std::string host, std::string port);
//         ssize_t read_from_server(int socket, char *buffer, size_t count);
//         ssize_t write_to_server(int socket, const char *buffer, size_t count);
//         void close_connection(int socket);
//     private:
//         mock_udp_factory *factory;
//         string hostname;
//         bool show_packets;
//     };

//     mock_udp_factory *factory;
// };

