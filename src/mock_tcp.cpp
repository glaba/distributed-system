#include "mock_tcp.h"
#include "serialization.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>

using std::string;
using std::unique_ptr;
using std::make_unique;

auto mock_tcp_factory::get_tcp_client(string const& host, int port) -> unique_ptr<tcp_client> {
    mock_tcp_client *retval = new mock_tcp_client(get_mock_udp_factory(), config->get_hostname());
    retval->setup_connection(host, port);
    return unique_ptr<tcp_client>(static_cast<tcp_client*>(retval));
}

auto mock_tcp_factory::get_tcp_server(int port) -> unique_ptr<tcp_server> {
    mock_tcp_server *retval = new mock_tcp_server(get_mock_udp_factory(), config->get_hostname());
    retval->setup_server(port);
    return unique_ptr<tcp_server>(static_cast<tcp_server*>(retval));
}

auto mock_tcp_factory::get_mock_udp_factory() -> mock_udp_factory* {
    // Create mock_udp_env if it has not yet been created
    unlocked<environment> mock_udp_env = mock_udp_env_lock();
    if (!mock_udp_env) {
        unlocked<mock_tcp_state> state = unlocked<mock_tcp_state>::dyn_cast(access_state());
        mock_udp_env.replace_data(state->udp_env_group->get_env());
    }
    mock_udp_factory *fac = dynamic_cast<mock_udp_factory*>(mock_udp_env->get<udp_factory>());
    fac->reinitialize(env); // Set the mock_udp_factory to get all its services from our env instead of the fake env
    return fac;
}

mock_tcp_factory::mock_tcp_server::~mock_tcp_server() {
    stop_server();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void mock_tcp_factory::mock_tcp_server::setup_server(int port_) {
    port = port_;
    id = static_cast<int32_t>(std::hash<string>()(hostname + std::to_string(port))) & 0x7FFFFFFF;

    client = factory->get_udp_client(hostname + "_server");
    server = factory->get_udp_server(hostname + "_server");
    server->start_server(port);

    running = true;
    std::thread msg_thread([this] {
        char buf[32768];
        unsigned length;

        while (running.load()) {
            // There is a tremendously unlikely race condition here if the mock_tcp_server destructor is called,
            // 500 milliseconds pass, and server is deleted all between the line above and the line below
            length = server->recv(buf, 32768);

            // Handle the condition where recv has already been called but the server has been stopped
            if (!running.load()) {
                continue;
            }

            // Assert that it at least contains the prefix and message length
            assert(length >= 5);

            char magic_byte = buf[0];
            uint32_t client_id = deserializer::read_uint32_from_char_buf(buf + 1);

            // Check the magic byte
            switch (magic_byte) {
                case initiate_magic_byte: {
                    unlocked<server_state> serv_state = serv_state_lock();

                    // Make sure we don't already have an open connection with this client
                    assert((serv_state->msg_queues.find(client_id) == serv_state->msg_queues.end() ||
                            serv_state->client_hostnames.find(client_id) == serv_state->client_hostnames.end()) &&
                            "Connection already open with this client");
                    serv_state->incoming_connections.push({client_id, string(buf + 5, length - 5)});
                    break;
                }
                case msg_magic_byte: {
                    unlocked<server_state> serv_state = serv_state_lock();

                    if (serv_state->msg_queues.find(client_id) == serv_state->msg_queues.end())
                        assert(false && "Received message from client before establishing connection");

                    serv_state->msg_queues[client_id].push(string(buf + 5, length - 5));
                    break;
                }
                case close_magic_byte: {
                    {
                        unlocked<server_state> serv_state = serv_state_lock();
                        // We have already closed the connection
                        if (serv_state->msg_queues.find(client_id) == serv_state->msg_queues.end())
                            break;
                    }

                    delete_connection(client_id);
                    break;
                }
                case accept_magic_byte: assert(false && "TCP server should not receive accept message");
                default: assert(false && "Invalid magic byte in mock TCP message");
            }
        }
    });
    msg_thread.detach();
}

void mock_tcp_factory::mock_tcp_server::stop_server() {
    assert(serv_state_lock()->client_hostnames.size() == 0 && "Not all connections closed before stopping server");

    running = false;
    server->stop_server();
}

auto mock_tcp_factory::mock_tcp_server::accept_connection() -> int {
    while (running.load()) {
        { // Atomic block to poll the queue
            unlocked<server_state> serv_state = serv_state_lock();

            if (serv_state->incoming_connections.size() > 0) {
                auto [client_id, client_hostname] = serv_state->incoming_connections.front();
                serv_state->incoming_connections.pop();

                serv_state->msg_queues[client_id] = std::queue<string>();
                serv_state->client_hostnames[client_id] = client_hostname;

                // Send a message back to the client saying that we've accepted the connection
                char accept_msg[5];
                accept_msg[0] = accept_magic_byte;
                serializer::write_uint32_to_char_buf(id, accept_msg + 1);
                client->send(client_hostname + std::to_string(client_id) + "->" + hostname + "_client", port,
                    string(accept_msg, 5));

                // We will use the client ID as the FD for the fake socket
                return client_id;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return -1;
}

void mock_tcp_factory::mock_tcp_server::close_connection(int client_socket) {
    unlocked<server_state> serv_state = serv_state_lock();

    if (serv_state->msg_queues.find(client_socket) == serv_state->msg_queues.end()) {
        // The connection was already closed
        return;
    }

    // Send a message saying the connection is closed
    char closed_msg[5];
    closed_msg[0] = close_magic_byte;
    serializer::write_uint32_to_char_buf(id, closed_msg + 1);
    uint32_t client_id = client_socket;
    client->send(serv_state->client_hostnames[client_socket] + std::to_string(client_id) + "->" + hostname + "_client",
        port, string(closed_msg, 5));

    // Delete all information
    delete_connection(client_socket);
}

void mock_tcp_factory::mock_tcp_server::delete_connection(int client_fd) {
    // Wait for msg_queue to become empty
    while (true) {
        {
            unlocked<server_state> serv_state = serv_state_lock();
            if (serv_state->msg_queues.find(client_fd) == serv_state->msg_queues.end()) {
                return;
            }
            if (serv_state->msg_queues[client_fd].size() == 0) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Remove all information about the client
    unlocked<server_state> serv_state = serv_state_lock();
    if (serv_state->msg_queues.find(client_fd) == serv_state->msg_queues.end()) {
        return;
    }

    serv_state->msg_queues.erase(client_fd);
    serv_state->client_hostnames.erase(client_fd);
}

auto mock_tcp_factory::mock_tcp_server::read_from_client(int client_fd) -> string {
    while (true) {
        { // Atomic block to poll the queue
            unlocked<server_state> serv_state = serv_state_lock();

            if (serv_state->msg_queues.find(client_fd) == serv_state->msg_queues.end()) {
                return "";
            }

            if (serv_state->msg_queues[client_fd].size() > 0) {
                string msg = serv_state->msg_queues[client_fd].front();
                serv_state->msg_queues[client_fd].pop();

                return msg;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

auto mock_tcp_factory::mock_tcp_server::write_to_client(int client_fd, string const& data) -> ssize_t {
    unlocked<server_state> serv_state = serv_state_lock();

    // Return 0 if the client is not connected
    if (serv_state->client_hostnames.find(client_fd) == serv_state->client_hostnames.end()) {
        return 0;
    }

    unique_ptr<char[]> msg = make_unique<char[]>(5 + data.size());

    // Create the message with the required fields
    msg[0] = msg_magic_byte;
    serializer::write_uint32_to_char_buf(id, msg.get() + 1);
    std::memcpy(msg.get() + 5, data.c_str(), data.size());

    uint32_t client_id = client_fd;
    client->send(serv_state->client_hostnames[client_fd] + std::to_string(client_id) + "->" + hostname + "_client",
        port, string(msg.get(), 5 + data.size()));
    return data.size();
}

auto mock_tcp_factory::mock_tcp_client::setup_connection(string const& host, int port) -> int {
    uint32_t server_id;

    { // Atomic block to access various data structures
        unlocked<client_state> cl_state = cl_state_lock();

        server_id = static_cast<int32_t>(std::hash<string>()(host + std::to_string(port))) & 0x7FFFFFFF;

        // Check that we don't already have a connection open to this server
        if (cl_state->server_hostnames.find(server_id) != cl_state->server_hostnames.find(server_id)) {
            assert(false && "Client already connected to this server");
        }

        // Each connection just pretends to be a different UDP host entirely
        cl_state->server_hostnames[server_id] = host;
        cl_state->server_ports[server_id] = port;
        clients[server_id] = factory->get_udp_client(hostname + std::to_string(id) + "->" + host + "_client");
        servers[server_id] = factory->get_udp_server(hostname + std::to_string(id) + "->" + host + "_client");
        servers[server_id]->start_server(port);

        // Send connection initiation message to server
        unique_ptr<char[]> initiation_msg = make_unique<char[]>(5 + hostname.size());
        initiation_msg[0] = initiate_magic_byte;
        serializer::write_uint32_to_char_buf(id, initiation_msg.get() + 1);
        std::memcpy(initiation_msg.get() + 5, hostname.c_str(), hostname.size());

        clients[server_id]->send(host + "_server", port, string(initiation_msg.get(), 5 + hostname.size()));

        cl_state->running[server_id] = true;
        std::thread msg_thread([this, server_id] {
            char buf[32768];
            unsigned length;

            while (true) {
                // There is a tremendously unlikely race condition here if stop_server is called and running is set to false,
                // 500 milliseconds pass, and servers[server_id] is deleted all between the line above and the line below
                length = servers[server_id]->recv(buf, 32768);

                { // Atomic block while we potentially touch the message queues
                    unlocked<client_state> cl_state = cl_state_lock();

                    // Handle the condition where recv has already been called but the server (or client) has been stopped
                    if (!cl_state->running[server_id].load()) {
                        return;
                    }
                }

                // Assert that it at least contains the prefix and message length
                assert(length >= 5);

                char magic_byte = buf[0];
                // Make sure that we are receiving messages from the correct ID
                assert(deserializer::read_uint32_from_char_buf(buf + 1) == server_id &&
                       "Received unexpected message from server on wrong UDP server instance");

                // Check the magic byte
                switch (magic_byte) {
                    case accept_magic_byte: {
                        unlocked<client_state> cl_state = cl_state_lock();

                        // Make sure we don't already have an open connection with this server
                        assert(cl_state->msg_queues.find(server_id) == cl_state->msg_queues.end() &&
                            "Connection already open with this server");
                        cl_state->msg_queues[server_id] = std::queue<string>();
                        break;
                    }
                    case msg_magic_byte: {
                        unlocked<client_state> cl_state = cl_state_lock();

                        if (cl_state->msg_queues.find(server_id) == cl_state->msg_queues.end())
                            assert(false && "Received message from server before establishing connection");

                        cl_state->msg_queues[server_id].push(string(buf + 5, length - 5));
                        break;
                    }
                    case close_magic_byte: {
                        {
                            unlocked<client_state> cl_state = cl_state_lock();
                            if (cl_state->msg_queues.find(server_id) == cl_state->msg_queues.end())
                                break; // We have already closed the connection
                        }

                        delete_connection(server_id);
                        return;
                    }
                    case initiate_magic_byte: assert(false && "TCP client should not receive initiate message");
                    default: assert(false && "Invalid magic byte in mock TCP message");
                }
            }
        });
        msg_thread.detach();
    }

    while (true) {
        { // Atomic block to access cl_state->msg_queues
            unlocked<client_state> cl_state = cl_state_lock();
            if (cl_state->msg_queues.find(server_id) != cl_state->msg_queues.end()) {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    fixed_socket = server_id;
    return server_id;
}

auto mock_tcp_factory::mock_tcp_client::read_from_server() -> string {
    while (true) {
        { // Atomic block to touch message queue
            unlocked<client_state> cl_state = cl_state_lock();

            if (cl_state->msg_queues.find(fixed_socket) == cl_state->msg_queues.end()) {
                return "";
            }

            if (cl_state->msg_queues[fixed_socket].size() > 0) {
                string msg = cl_state->msg_queues[fixed_socket].front();
                cl_state->msg_queues[fixed_socket].pop();

                return msg;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

auto mock_tcp_factory::mock_tcp_client::write_to_server(string const& data) -> ssize_t {
    unlocked<client_state> cl_state = cl_state_lock();

    // Return 0 if not connected
    if (cl_state->server_hostnames.find(fixed_socket) == cl_state->server_hostnames.end()) {
        return 0;
    }

    unique_ptr<char[]> msg = make_unique<char[]>(5 + data.size());

    // Create the message with the required fields
    msg[0] = msg_magic_byte;
    serializer::write_uint32_to_char_buf(id, msg.get() + 1);
    std::memcpy(msg.get() + 5, data.c_str(), data.size());
    clients[fixed_socket]->send(cl_state->server_hostnames[fixed_socket] + "_server",
        cl_state->server_ports[fixed_socket], string(msg.get(), 5 + data.size()));
    return data.size();
}

void mock_tcp_factory::mock_tcp_client::close_connection() {
    unlocked<client_state> cl_state = cl_state_lock();

    // The connection has already been closed
    if (cl_state->server_hostnames.find(fixed_socket) == cl_state->server_hostnames.end()) {
        return;
    }

    // Send a message saying we are closing the connection
    char closed_msg[5];
    closed_msg[0] = close_magic_byte;
    serializer::write_uint32_to_char_buf(id, closed_msg + 1);
    clients[fixed_socket]->send(cl_state->server_hostnames[fixed_socket] + "_server",
        cl_state->server_ports[fixed_socket], string(closed_msg, 5));

    // Delete all data related to the socket
    delete_connection(fixed_socket);
}

void mock_tcp_factory::mock_tcp_client::delete_connection(int socket) {
    // Wait for all messages to be delivered
    while (true) {
        {
            unlocked<client_state> cl_state = cl_state_lock();
            if (cl_state->server_hostnames.find(socket) == cl_state->server_hostnames.end()) {
                return;
            }
            if (cl_state->msg_queues[socket].size() == 0) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    { // Remove all data about the server
        unlocked<client_state> cl_state = cl_state_lock();
        if (cl_state->server_hostnames.find(socket) == cl_state->server_hostnames.end()) {
            return;
        }
        if (cl_state->running[socket].load()) {
            cl_state->running[socket] = false;
            servers[socket]->stop_server();
        }
    }

    // Wait for the server to stop (should actually take ~20ms, but we give some extra time)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        unlocked<client_state> cl_state = cl_state_lock();
        if (cl_state->server_hostnames.find(socket) == cl_state->server_hostnames.end()) {
            return;
        }

        // Delete all information
        servers.erase(socket);
        clients.erase(socket);
        cl_state->running.erase(socket);
        cl_state->msg_queues.erase(socket);
        cl_state->server_hostnames.erase(socket);
        cl_state->server_ports.erase(socket);
    }
}

register_test_service<tcp_factory, mock_tcp_factory> register_mock_tcp_factory;
