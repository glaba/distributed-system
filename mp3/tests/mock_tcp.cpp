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
    "Tests passing messages between one TCP client and one TCP server",
    4, [] (logger::log_level level)
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

    // Start the server in its own thread
    bool server_complete = false;
    std::thread server_thread([&server, &server_complete] {
        server->setup_server(1234);

        int fd = server->accept_connection();

        for (int i = 0; i < 10; i++) {
            std::string msg = server->read_from_client(fd);
            assert(msg == std::to_string(i));
            server->write_to_client(fd, std::to_string(i));
        }
        server_complete = true;
    });
    server_thread.detach();

    // Wait a little bit for the server to get setup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    bool client_complete = false;
    std::thread client_thread([&client, &client_complete] {
        int fd = client->setup_connection("server", 1234);
        for (int i = 0; i < 10; i++) {
            client->write_to_server(fd, std::to_string(i));
            std::string msg = client->read_from_server(fd);
            assert(msg == std::to_string(i));
        }
        client_complete = true;
    });
    client_thread.detach();

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    assert(client_complete && server_complete);
});

testing::register_test one_client_one_server_ports("mock_tcp.one_client_one_server_ports",
    "Tests passing messages between one TCP client and multiple TCP server on the same machine with different ports",
    4, [] (logger::log_level level)
{
    const unsigned NUM_SERVERS = 5;

    environment_group env_group(true);
    unique_ptr<environment> client_env = env_group.get_env();
    unique_ptr<environment> server_env = env_group.get_env();
    client_env->get<configuration>()->set_hostname("client");
    server_env->get<configuration>()->set_hostname("server");
    client_env->get<logger_factory>()->configure(level);
    server_env->get<logger_factory>()->configure(level);

    unique_ptr<tcp_client> client = client_env->get<tcp_factory>()->get_tcp_client();
    std::vector<unique_ptr<tcp_server>> servers;
    for (unsigned i = 0; i < NUM_SERVERS; i++) {
        servers.push_back(server_env->get<tcp_factory>()->get_tcp_server());
    }

    // Start the servers in their own thread
    std::vector<bool> servers_complete;
    for (unsigned i = 0; i < NUM_SERVERS; i++) {
        servers_complete.push_back(false);
        std::thread server_thread([&servers, &servers_complete, i] {
            servers[i]->setup_server(i + 1); // A port of 0 is invalid

            int fd = servers[i]->accept_connection();

            for (int j = 0; j < 10; j++) {
                std::string msg = servers[i]->read_from_client(fd);
                assert(msg == std::to_string(j));
                servers[i]->write_to_client(fd, std::to_string(j));
            }
            servers_complete[i] = true;
        });
        server_thread.detach();
    }

    // Wait a little bit for the servers to get setup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::vector<bool> clients_complete;
    for (unsigned i = 0; i < NUM_SERVERS; i++) {
        clients_complete.push_back(false);
        std::thread client_thread([&client, &clients_complete, i] {
            int fd = client->setup_connection("server", i + 1);
            for (int j = 0; j < 10; j++) {
                client->write_to_server(fd, std::to_string(j));
                std::string msg = client->read_from_server(fd);
                assert(msg == std::to_string(j));

                // Add a delay to ensure that messages are interleaved
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            clients_complete[i] = true;
        });
        client_thread.detach();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    for (unsigned i = 0; i < NUM_SERVERS; i++) {
        assert(clients_complete[i]);
        assert(servers_complete[i]);
    }
});

testing::register_test server_after_client("mock_tcp.server_after_client",
    "Tests that even if a server calls accept_connection after a client calls setup_connection, it works",
    3, [] (logger::log_level level)
{
    environment_group env_group(true);
    unique_ptr<environment> client_env = env_group.get_env();
    unique_ptr<environment> server_env = env_group.get_env();
    client_env->get<configuration>()->set_hostname("client");
    server_env->get<configuration>()->set_hostname("server");
    client_env->get<logger_factory>()->configure(level);
    server_env->get<logger_factory>()->configure(level);

    dynamic_cast<mock_tcp_factory*>(client_env->get<tcp_factory>())->show_packets();
    dynamic_cast<mock_tcp_factory*>(server_env->get<tcp_factory>())->show_packets();
    unique_ptr<tcp_client> client = client_env->get<tcp_factory>()->get_tcp_client();
    unique_ptr<tcp_server> server = server_env->get<tcp_factory>()->get_tcp_server();

    // Start the server in its own thread
    bool server_complete = false;
    std::thread server_thread([&server, &server_complete] {
        server->setup_server(1234);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        int fd = server->accept_connection();

        for (int i = 0; i < 10; i++) {
            std::string msg = server->read_from_client(fd);
            assert(msg == std::to_string(i));
            server->write_to_client(fd, std::to_string(i));
        }
        server_complete = true;
    });
    server_thread.detach();

    bool client_complete = false;
    std::thread client_thread([&client, &client_complete] {
        int fd = client->setup_connection("server", 1234);
        for (int i = 0; i < 10; i++) {
            client->write_to_server(fd, std::to_string(i));
            std::string msg = client->read_from_server(fd);
            assert(msg == std::to_string(i));
        }
        client_complete = true;
    });
    client_thread.detach();

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    assert(client_complete && server_complete);
});

testing::register_test closing_connection("mock_tcp.closing_connection",
    "Tests that closing connections behaves as expected",
    9, [] (logger::log_level level)
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
        // Start the server in its own thread
        bool server_complete = false;
        std::thread server_thread([&server, i, &server_complete] {
            int fd = server->accept_connection();

            for (int i = 0; i < 10; i++) {
                std::string msg = server->read_from_client(fd);
                assert(msg == std::to_string(i));
                server->write_to_client(fd, std::to_string(i));
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
            }
            server_complete = true;
        });
        server_thread.detach();

        // Wait for server
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Use the current thread for the client
        bool client_complete = false;
        std::thread client_thread([&client, i, &client_complete] {
            int fd = client->setup_connection("server", 1234);
            for (int i = 0; i < 10; i++) {
                client->write_to_server(fd, std::to_string(i));
                std::string msg = client->read_from_server(fd);
                assert(msg == std::to_string(i));
            }

            if (i == 0) {
                // We should now find that the connection to the server is closed
                assert(client->read_from_server(fd) == "");
                assert(client->write_to_server(fd, "LLJ") == 0);
            } else {
                // Client closes connection
                // Add delay to avoid race condition, as with the server
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                client->close_connection(fd);
            }
            client_complete = true;
        });
        client_thread.detach();

        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        assert(client_complete && server_complete);
    }
});

testing::register_test one_client_n_servers("mock_tcp.one_client_n_servers",
    "Tests passing messages between a single TCP client and multiple TCP servers",
    6, [] (logger::log_level level)
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

    // Start the servers in their own thread
    std::vector<bool> servers_complete;
    for (int i = 0; i < NUM_SERVERS; i++) {
        servers_complete.push_back(false);
        std::thread server_thread([&servers, i, &servers_complete] {
            servers[i]->setup_server(1234);

            int fd = servers[i]->accept_connection();

            for (int j = 0; j < 10; j++) {
                std::string msg = servers[i]->read_from_client(fd);
                assert(msg == std::to_string(10 * i + j));
                servers[i]->write_to_client(fd, std::to_string(10 * i + j));
            }

            // Add delay to avoid race condition between closing connection and sending final message
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            servers[i]->close_connection(fd);
            servers[i]->stop_server();
            servers_complete[i] = true;
        });
        server_thread.detach();
    }

    // Wait a little bit for the server to get setup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Open connections to all the servers
    bool client_complete = false;
    std::thread client_thread([&client, &client_complete] {
        std::vector<uint32_t> fds;
        for (int i = 0; i < NUM_SERVERS; i++) {
            fds.push_back(client->setup_connection("server" + std::to_string(i), 1234));
        }

        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < NUM_SERVERS; j++) {
                client->write_to_server(fds[j], std::to_string(10 * j + i));
            }
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
        client_complete = true;
    });
    client_thread.detach();

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    assert(client_complete);
    for (unsigned i = 0; i < servers_complete.size(); i++) {
        assert(servers_complete[i]);
    }
});

testing::register_test n_clients_one_server("mock_tcp.n_clients_one_server",
    "Tests passing messages between a multiple TCP clients and a single TCP server",
    6, [] (logger::log_level level)
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
                }
            });
            client_thread.detach();
        }
    });
    server_thread.detach();

    // Wait a bit for the server to set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Start the clients in their own thread
    std::vector<bool> clients_complete;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients_complete.push_back(false);
        std::thread client_thread([&clients, i, &clients_complete] {
            int fd = clients[i]->setup_connection("server", 1234);

            for (int j = 0; j < 10; j++) {
                clients[i]->write_to_server(fd, std::to_string(j));
                std::string msg = clients[i]->read_from_server(fd);
                assert(msg == std::to_string(j));
            }

            clients[i]->close_connection(fd);
            clients_complete[i] = true;
        });
        client_thread.detach();
    }

    // Wait for all communication to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    for (unsigned i = 0; i < clients_complete.size(); i++) {
        assert(clients_complete[i]);
    }
});

testing::register_test n_clients_same_machine_one_server("mock_tcp.n_clients_same_machine_one_server",
    "Tests passing messages between multiple TCP clients on the same machine and a single TCP server",
    6, [] (logger::log_level level)
{
    const int NUM_CLIENTS = 5;

    environment_group env_group(true);
    unique_ptr<environment> client_env = env_group.get_env();
    unique_ptr<environment> server_env = env_group.get_env();

    client_env->get<configuration>()->set_hostname("client");
    client_env->get<logger_factory>()->configure(level);
    server_env->get<configuration>()->set_hostname("server");
    server_env->get<logger_factory>()->configure(level);

    unique_ptr<tcp_server> server = server_env->get<tcp_factory>()->get_tcp_server();
    std::vector<unique_ptr<tcp_client>> clients;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients.push_back(client_env->get<tcp_factory>()->get_tcp_client());
    }

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
                }
            });
            client_thread.detach();
        }
    });
    server_thread.detach();

    // Wait a bit for the server to set up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Start the clients in their own thread
    std::vector<bool> clients_complete;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients_complete.push_back(false);
        std::thread client_thread([&clients, i, &clients_complete] {
            int fd = clients[i]->setup_connection("server", 1234);

            for (int j = 0; j < 10; j++) {
                clients[i]->write_to_server(fd, std::to_string(j));
                std::string msg = clients[i]->read_from_server(fd);
                assert(msg == std::to_string(j));
            }

            clients[i]->close_connection(fd);
            clients_complete[i] = true;
        });
        client_thread.detach();
    }

    // Wait for all communication to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    for (unsigned i = 0; i < clients_complete.size(); i++) {
        assert(clients_complete[i]);
    }
});
