#include "mock_tcp.h"
#include "serialization.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>

using std::unique_ptr;
using std::make_unique;

unique_ptr<tcp_client_intf> mock_tcp_factory::get_mock_tcp_client(string hostname, bool show_packets) {
    return make_unique<mock_tcp_client>(factory.get(), hostname, show_packets);
}

unique_ptr<tcp_server_intf> mock_tcp_factory::get_mock_tcp_server(string hostname, bool show_packets) {
    return make_unique<mock_tcp_server>(factory.get(), hostname, show_packets);
}

mock_tcp_factory::mock_tcp_server::~mock_tcp_server() {
    {
        std::lock_guard<std::recursive_mutex> guard(msg_mutex);

        if (running.load()) {
            stop_server();
        }
    }

    // 500ms delay to reduce chance of race condition in msg_thread to essentially 0
    // This should not be protected by the lock so that msg_thread gets its chance to finish up its work and quit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void mock_tcp_factory::mock_tcp_server::setup_server(int port_) {
    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

    port = port_;

    client = factory->get_mock_udp_client(hostname + "_server", show_packets, 0.0);
    server = factory->get_mock_udp_server(hostname + "_server");
    server->start_server(port);

    running = true;
    msg_thread = std::thread([this] {
        char buf[1024];
        unsigned length;

        while (running.load()) {
            // There is a tremendously unlikely race condition here if the mock_tcp_server destructor is called,
            // 500 milliseconds pass, and server is deleted all between the line above and the line below
            length = server->recv(buf, 1024);

            { // Atomic block while we potentially touch the message queues
                std::lock_guard<std::recursive_mutex> guard(msg_mutex);

                // Handle the condition where recv has already been called but the server has been stopped
                if (!running.load()) {
                    continue;
                }

                // Assert that it at least contains the prefix and message length
                assert(length >= 5);

                char magic_byte = buf[0];
                uint32_t client_id = serialization::read_uint32_from_char_buf(buf + 1);

                // Check the magic byte
                switch (magic_byte) {
                    case initiate_magic_byte: {
                        // Make sure we don't already have an open connection with this client
                        assert((msg_queues.find(client_id) == msg_queues.end() ||
                                client_hostnames.find(client_id) == client_hostnames.end()) &&
                                "Connection already open with this client");
                        incoming_connections.push(std::make_tuple(client_id, std::string(buf + 5, length - 5)));
                        break;
                    }
                    case msg_magic_byte: {
                        if (msg_queues.find(client_id) == msg_queues.end())
                            assert(false && "Received message from client before establishing connection");

                        msg_queues[client_id].push(std::string(buf + 5, length - 5));
                        break;
                    }
                    case close_magic_byte: {
                        if (msg_queues.find(client_id) == msg_queues.end())
                            assert(false && "Received connection closed message from server before establishing connection");

                        delete_connection(client_id);
                        break;
                    }
                    case accept_magic_byte: assert(false && "TCP server should not receive accept message");
                    default: assert(false && "Invalid magic byte in mock TCP message");
                }
            }
        }
    });
    msg_thread.detach();
}

void mock_tcp_factory::mock_tcp_server::stop_server() {
    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

    // Close all open connections with client (use the vector to avoid modifying container while iterating)
    std::vector<uint32_t> open_fds;
    for (auto client_info : client_hostnames) {
        open_fds.push_back(client_info.first);
    }

    for (uint32_t fd : open_fds) {
        close_connection(fd);
    }

    running = false;
    server->stop_server();
}

int mock_tcp_factory::mock_tcp_server::accept_connection() {
    while (true) {
        { // Atomic block to poll the queue
            std::lock_guard<std::recursive_mutex> guard(msg_mutex);

            if (incoming_connections.size() > 0) {
                std::tuple<uint32_t, std::string> client_info = incoming_connections.front();
                incoming_connections.pop();

                uint32_t client_id = std::get<0>(client_info);
                std::string client_hostname = std::get<1>(client_info);

                msg_queues[client_id] = std::queue<std::string>();
                client_hostnames[client_id] = client_hostname;

                // Send a message back to the client saying that we've accepted the connection
                char accept_msg[5];
                accept_msg[0] = accept_magic_byte;
                serialization::write_uint32_to_char_buf(id, accept_msg + 1);
                client->send(client_hostname + "->" + hostname + "_client", port, accept_msg, 5);

                // We will use the client ID as the FD for the fake socket
                return client_id;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void mock_tcp_factory::mock_tcp_server::close_connection(int client_socket) {
    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

    assert(msg_queues.find(client_socket) != msg_queues.end() && "Client was not connected to begin with");

    // Send a message saying the connection is closed
    char closed_msg[5];
    closed_msg[0] = close_magic_byte;
    serialization::write_uint32_to_char_buf(id, closed_msg + 1);
    client->send(client_hostnames[client_socket] + "->" + hostname + "_client", port, closed_msg, 5);

    // Delete all information
    delete_connection(client_socket);
}

void mock_tcp_factory::mock_tcp_server::delete_connection(int client_fd) {
    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

    assert(msg_queues.find(client_fd) != msg_queues.end() && "Client was not connected to begin with");

    // Remove all information about the client
    msg_queues.erase(client_fd);
    client_hostnames.erase(client_fd);
}

std::string mock_tcp_factory::mock_tcp_server::read_from_client(int client_fd) {
    while (true) {
        { // Atomic block to poll the queue
            std::lock_guard<std::recursive_mutex> guard(msg_mutex);

            if (msg_queues.find(client_fd) == msg_queues.end()) {
                return "";
            }

            if (msg_queues[client_fd].size() > 0) {
                std::string msg = msg_queues[client_fd].front();
                msg_queues[client_fd].pop();

                return msg;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

ssize_t mock_tcp_factory::mock_tcp_server::write_to_client(int client_fd, std::string data) {
    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

    // Return 0 if the client is not connected
    if (client_hostnames.find(client_fd) == client_hostnames.end()) {
        return 0;
    }

    unique_ptr<char[]> msg = make_unique<char[]>(5 + data.size());

    // Create the message with the required fields
    msg[0] = msg_magic_byte;
    serialization::write_uint32_to_char_buf(id, msg.get() + 1);
    std::strncpy(msg.get() + 5, data.c_str(), data.size());

    client->send(client_hostnames[client_fd] + "->" + hostname + "_client", port, msg.get(), 5 + data.size());
    return data.size();
}

mock_tcp_factory::mock_tcp_client::~mock_tcp_client() {
    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

    // Close all connections
    std::vector<uint32_t> open_fds;
    for (auto server_info : server_hostnames) {
        open_fds.push_back(server_info.first);
    }

    for (uint32_t fd : open_fds) {
        close_connection(fd);
    }
}

int mock_tcp_factory::mock_tcp_client::setup_connection(std::string host, int port_) {
    uint32_t server_id;

    { // Atomic block to access various data structures
        std::lock_guard<std::recursive_mutex> guard(msg_mutex);

        server_id = std::hash<std::string>()(host);

        // Check that we don't already have a connection open to this server
        if (server_hostnames.find(server_id) != server_hostnames.find(server_id)) {
            assert(false && "Client already connected to this server");
        }

        port = port_;

        // Each connection just pretends to be a different UDP host entirely
        server_hostnames[server_id] = host;
        clients[server_id] = factory->get_mock_udp_client(hostname + "->" + host + "_client", show_packets, 0.0);
        servers[server_id] = factory->get_mock_udp_server(hostname + "->" + host + "_client");
        servers[server_id]->start_server(port);

        // Send connection initiation message to server
        unique_ptr<char[]> initiation_msg = make_unique<char[]>(5 + hostname.size());
        initiation_msg[0] = initiate_magic_byte;
        serialization::write_uint32_to_char_buf(id, initiation_msg.get() + 1);
        std::strncpy(initiation_msg.get() + 5, hostname.c_str(), hostname.size());
        clients[server_id]->send(host + "_server", port, initiation_msg.get(), 5 + hostname.size());

        running[server_id] = true;
        msg_threads[server_id] = std::thread([this, server_id] {
            char buf[1024];
            unsigned length;

            while (running[server_id].load()) {
                // There is a tremendously unlikely race condition here if stop_server is called and running is set to false,
                // 500 milliseconds pass, and servers[server_id] is deleted all between the line above and the line below
                length = servers[server_id]->recv(buf, 1024);

                { // Atomic block while we potentially touch the message queues
                    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

                    // Handle the condition where recv has already been called but the server (or client) has been stopped
                    if (!running[server_id].load()) {
                        return;
                    }

                    // Assert that it at least contains the prefix and message length
                    assert(length >= 5);

                    char magic_byte = buf[0];
                    // Make sure that we are receiving messages from the correct ID
                    assert(serialization::read_uint32_from_char_buf(buf + 1) == server_id &&
                           "Received unexpected message from server on wrong UDP server instance");

                    // Check the magic byte
                    switch (magic_byte) {
                        case accept_magic_byte: {
                            // Make sure we don't already have an open connection with this server
                            assert(msg_queues.find(server_id) == msg_queues.end() && "Connection already open with this server");
                            msg_queues[server_id] = std::queue<std::string>();
                            break;
                        }
                        case msg_magic_byte: {
                            if (msg_queues.find(server_id) == msg_queues.end())
                                assert(false && "Received message from server before establishing connection");

                            msg_queues[server_id].push(std::string(buf + 5, length - 5));
                            break;
                        }
                        case close_magic_byte: {
                            if (msg_queues.find(server_id) == msg_queues.end())
                                assert(false && "Received connection closed message from server before establishing connection");

                            delete_connection(server_id);
                            return;
                        }
                        case initiate_magic_byte: assert(false && "TCP client should not receive initiate message");
                        default: assert(false && "Invalid magic byte in mock TCP message");
                    }
                }
            }
        });
        msg_threads[server_id].detach();
    }

    while (true) {
        { // Atomic block to access msg_queues
            std::lock_guard<std::recursive_mutex> guard(msg_mutex);
            if (msg_queues.find(server_id) != msg_queues.end()) {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return server_id;
}

std::string mock_tcp_factory::mock_tcp_client::read_from_server(int socket) {
    while (true) {
        { // Atomic block to touch message queue
            std::lock_guard<std::recursive_mutex> guard(msg_mutex);

            if (msg_queues.find(socket) == msg_queues.end()) {
                return "";
            }

            if (msg_queues[socket].size() > 0) {
                std::string msg = msg_queues[socket].front();
                msg_queues[socket].pop();

                return msg;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

ssize_t mock_tcp_factory::mock_tcp_client::write_to_server(int socket, std::string data) {
    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

    // Return 0 if not connected
    if (server_hostnames.find(socket) == server_hostnames.end()) {
        return 0;
    }

    unique_ptr<char[]> msg = make_unique<char[]>(5 + data.size());

    // Create the message with the required fields
    msg[0] = msg_magic_byte;
    serialization::write_uint32_to_char_buf(id, msg.get() + 1);
    std::strncpy(msg.get() + 5, data.c_str(), data.size());
    clients[socket]->send(server_hostnames[socket] + "_server", port, msg.get(), 5 + data.size());
    return data.size();
}

void mock_tcp_factory::mock_tcp_client::close_connection(int socket) {
    std::lock_guard<std::recursive_mutex> guard(msg_mutex);

    assert(server_hostnames.find(socket) != server_hostnames.end() && "Socket was not opened to begin with");

    // Send a message saying we are closing the connection
    char closed_msg[5];
    closed_msg[0] = close_magic_byte;
    serialization::write_uint32_to_char_buf(id, closed_msg + 1);
    clients[socket]->send(server_hostnames[socket] + "_server", port, closed_msg, 5);

    // Delete all data related to the socket
    delete_connection(socket);
}

void mock_tcp_factory::mock_tcp_client::delete_connection(int socket) {
    {
        std::lock_guard<std::recursive_mutex> guard(msg_mutex);

        assert(server_hostnames.find(socket) != server_hostnames.end() && "Socket was not opened to begin with");

        // Remove all data about the server
        running[socket] = false;
        servers[socket]->stop_server();
    }

    // Wait for the server to stop (should actually take ~20ms, but we give some extra time)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        std::lock_guard<std::recursive_mutex> guard(msg_mutex);

        // Delete all information
        servers.erase(socket);
        clients.erase(socket);
        msg_threads.erase(socket);
        running.erase(socket);
        msg_queues.erase(socket);
        server_hostnames.erase(socket);
    }
}
