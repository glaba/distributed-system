#include "test.h"
#include "mock_tcp.h"

#include <memory>
#include <thread>
#include <chrono>
#include <cassert>

using std::unique_ptr;
using std::make_unique;

testing::register_test mock_tcp("mock_tcp.basic_client_server",
    "Tests passing messages between a single TCP client and a single TCP server", [] (logger *lg)
{
    unique_ptr<mock_tcp_factory> factory = make_unique<mock_tcp_factory>();

    unique_ptr<tcp_client_intf> client = factory->get_mock_tcp_client("client", false);
    unique_ptr<tcp_server_intf> server = factory->get_mock_tcp_server("server", false);

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

        // Add delay to avoid race condition between closing connection and sending final message
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        server->close_connection(fd);
        server->stop_server();
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

    // We should now find that the connection to the server is closed
    assert(client->read_from_server(fd) == "");
    assert(client->write_to_server(fd, "LLJ") == 0);
    std::cout << "]" << std::endl;
});