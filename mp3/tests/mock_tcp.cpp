#include "test.h"
#include "mock_tcp.h"
#include "environment.h"
#include "configuration.h"

#include <memory>
#include <thread>
#include <chrono>
#include <cassert>

using std::unique_ptr;
using std::make_unique;

testing::register_test one_client_one_server("mock_tcp.one_client_one_server",
    "Tests passing messages between one TCP client and one TCP server", [] (logger::log_level level)
{
    environment_group env_group(true);
    unique_ptr<environment> client_env = env_group.get_env();
    unique_ptr<environment> server_env = env_group.get_env();
    client_env->get<configuration>()->set_hostname("client");
    server_env->get<configuration>()->set_hostname("server");
    client_env->get<logger_factory>()->configure(level);
    server_env->get<logger_factory>()->configure(level);

    unique_ptr<tcp_client> client = client_env->get<tcp_factory>()->get_tcp_client();
    unique_ptr<tcp_server> server = server_env->get<tcp_factory>()->get_tcp_server();

    std::cout << "[ " << std::flush;

    // Start the server in its own thread
    std::thread server_thread([&server] {
        server->setup_server(1234);

        int fd = server->accept_connection();

        for (int i = 0; i < 10; i++) {
            std::string msg = server->read_from_client(fd);
            assert(msg == std::to_string(i));
            server->write_to_client(fd, std::to_string(i));
            std::cout << "<- " << std::flush;
        }
    });
    server_thread.detach();

    // Wait a little bit for the server to get setup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Use the current thread for the client
    int fd = client->setup_connection("server", 1234);
    for (int i = 0; i < 10; i++) {
        client->write_to_server(fd, std::to_string(i));
        std::cout << "->" << std::flush;
        std::string msg = client->read_from_server(fd);
        assert(msg == std::to_string(i));
    }

    std::cout << "]" << std::endl;
});

testing::register_test closing_connection("mock_tcp.closing_connection",
    "Tests that closing connections behaves as expected", [] (logger::log_level level)
{
    environment_group env_group(true);
    unique_ptr<environment> client_env = env_group.get_env();
    unique_ptr<environment> server_env = env_group.get_env();
    client_env->get<configuration>()->set_hostname("client");
    server_env->get<configuration>()->set_hostname("server");
    client_env->get<logger_factory>()->configure(level);
    server_env->get<logger_factory>()->configure(level);

    unique_ptr<tcp_client> client = client_env->get<tcp_factory>()->get_tcp_client();
    unique_ptr<tcp_server> server = server_env->get<tcp_factory>()->get_tcp_server();
    server->setup_server(1234);

    // Wait a little bit for the server to get setup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // On the first iteration, the server will close the connection, and the client on the second
    for (int i = 0; i < 2; i++) {
        std::cout << ((i == 0) ? "Server" : "Client") << " closing connection: [ " << std::flush;

        // Start the server in its own thread
        std::thread server_thread([&server, i] {
            int fd = server->accept_connection();

            for (int i = 0; i < 10; i++) {
                std::string msg = server->read_from_client(fd);
                assert(msg == std::to_string(i));
                server->write_to_client(fd, std::to_string(i));
                std::cout << "<- " << std::flush;
            }

            if (i == 0) {
                // Server closes connection
                // Add delay to avoid race condition between closing connection and sending final message
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                server->close_connection(fd);
            } else {
                // We should now find that the connection to the client is closed
                assert(server->read_from_client(fd) == "");
                assert(server->write_to_client(fd, "LLJ") == 0);
                std::cout << "]" << std::endl;
            }
        });
        server_thread.detach();

        // Use the current thread for the client
        int fd = client->setup_connection("server", 1234);
        for (int i = 0; i < 10; i++) {
            client->write_to_server(fd, std::to_string(i));
            std::cout << "->" << std::flush;
            std::string msg = client->read_from_server(fd);
            assert(msg == std::to_string(i));
        }

        if (i == 0) {
            // We should now find that the connection to the server is closed
            assert(client->read_from_server(fd) == "");
            assert(client->write_to_server(fd, "LLJ") == 0);
            std::cout << "]" << std::endl;
        } else {
            // Client closes connection
            // Add delay to avoid race condition, as with the server
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            client->close_connection(fd);
        }
    }
});

testing::register_test one_client_n_servers("mock_tcp.one_client_n_servers",
    "Tests passing messages between a single TCP client and multiple TCP servers", [] (logger::log_level level)
{
    const int NUM_SERVERS = 5;

    environment_group env_group(true);
    std::vector<unique_ptr<environment>> server_envs = env_group.get_envs(NUM_SERVERS);
    unique_ptr<environment> client_env = env_group.get_env();

    client_env->get<configuration>()->set_hostname("client");
    client_env->get<logger_factory>()->configure(level);
    for (int i = 0; i < NUM_SERVERS; i++) {
        server_envs[i]->get<configuration>()->set_hostname("server" + std::to_string(i));
        server_envs[i]->get<logger_factory>()->configure(level);
    }

    unique_ptr<tcp_client> client = client_env->get<tcp_factory>()->get_tcp_client();

    std::vector<unique_ptr<tcp_server>> servers;
    for (int i = 0; i < NUM_SERVERS; i++) {
        servers.push_back(server_envs[i]->get<tcp_factory>()->get_tcp_server());
    }

    std::cout << "[" << std::flush;

    // Start the servers in their own thread
    for (int i = 0; i < NUM_SERVERS; i++) {
        std::thread server_thread([&servers, i] {
            servers[i]->setup_server(1234);

            int fd = servers[i]->accept_connection();

            for (int j = 0; j < 10; j++) {
                std::string msg = servers[i]->read_from_client(fd);
                assert(msg == std::to_string(10 * i + j));
                servers[i]->write_to_client(fd, std::to_string(10 * i + j));
                std::cout << "<-" << std::flush;
            }

            // Add delay to avoid race condition between closing connection and sending final message
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            servers[i]->close_connection(fd);
            servers[i]->stop_server();
        });
        server_thread.detach();
    }

    // Wait a little bit for the server to get setup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Use the current thread for the client
    // Open connections to all the servers
    std::vector<uint32_t> fds;
    for (int i = 0; i < NUM_SERVERS; i++) {
        fds.push_back(client->setup_connection("server" + std::to_string(i), 1234));
    }

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < NUM_SERVERS; j++) {
            client->write_to_server(fds[j], std::to_string(10 * j + i));
        }
        std::cout << " ->" << std::flush;
        for (int j = 0; j < NUM_SERVERS; j++) {
            std::string msg = client->read_from_server(fds[j]);
            assert(msg == std::to_string(10 * j + i));
        }
    }

    // We should now find that the connection to the servers are closed
    for (int i = 0; i < NUM_SERVERS; i++) {
        assert(client->read_from_server(fds[i]) == "");
        assert(client->write_to_server(fds[i], "LLJ") == 0);
    }
    std::cout << " ]" << std::endl;
});

testing::register_test n_clients_one_server("mock_tcp.n_clients_one_server",
    "Tests passing messages between a multiple TCP clients and a single TCP server", [] (logger::log_level level)
{
    const int NUM_CLIENTS = 5;

    environment_group env_group(true);
    std::vector<unique_ptr<environment>> client_envs = env_group.get_envs(NUM_CLIENTS);
    unique_ptr<environment> server_env = env_group.get_env();

    server_env->get<configuration>()->set_hostname("server");
    server_env->get<logger_factory>()->configure(level);
    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_envs[i]->get<configuration>()->set_hostname("client" + std::to_string(i));
        client_envs[i]->get<logger_factory>()->configure(level);
    }

    unique_ptr<tcp_server> server = server_env->get<tcp_factory>()->get_tcp_server();

    std::vector<unique_ptr<tcp_client>> clients;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients.push_back(client_envs[i]->get<tcp_factory>()->get_tcp_client());
    }

    std::cout << "[ " << std::flush;

    // Start the server in its own thread
    std::thread server_thread([&server, NUM_CLIENTS] {
        server->setup_server(1234);

        for (int i = 0; i < NUM_CLIENTS; i++) {
            int fd = server->accept_connection();

            // Spin off a thread for each client
            std::thread client_thread([&server, fd] {
                for (int j = 0; j < 10; j++) {
                    std::string msg = server->read_from_client(fd);
                    assert(msg == std::to_string(j));
                    server->write_to_client(fd, std::to_string(j));
                    std::cout << "<-" << std::flush;
                }
            });
            client_thread.detach();
        }
    });
    server_thread.detach();

    // Wait a bit for the server to set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Start the clients in their own thread
    for (int i = 0; i < NUM_CLIENTS; i++) {
        std::thread client_thread([&clients, i] {
            int fd = clients[i]->setup_connection("server", 1234);

            for (int j = 0; j < 10; j++) {
                clients[i]->write_to_server(fd, std::to_string(j));
                std::cout << "->" << std::flush;
                std::string msg = clients[i]->read_from_server(fd);
                assert(msg == std::to_string(j));
            }

            clients[i]->close_connection(fd);
        });
        client_thread.detach();
    }

    // Wait for all communication to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << " ]" << std::endl;
});
