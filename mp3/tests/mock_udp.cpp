#include "test.h"
#include "mock_udp.h"

#include <thread>
#include <memory>
#include <iostream>
#include <chrono>

using std::unique_ptr;
using std::make_unique;

testing::register_test mock_udp("mock_udp.mock_udp",
    "Tests passing messages between two client / server pairs", [] (logger *lg)
{
    unique_ptr<mock_udp_factory> fac = make_unique<mock_udp_factory>();

    unique_ptr<udp_client_intf> h1_client = fac->get_mock_udp_client("h1", false, 0.0);
    unique_ptr<udp_server_intf> h1_server = fac->get_mock_udp_server("h1");
    unique_ptr<udp_client_intf> h2_client = fac->get_mock_udp_client("h2", false, 0.0);
    unique_ptr<udp_server_intf> h2_server = fac->get_mock_udp_server("h2");

    volatile bool end = false;

    std::cout << "[ ";

    std::thread h1c([&h1_client] {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        for (char i = 0; i < 20; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            h1_client->send("h2", 1234, const_cast<char*>(std::string(1, i).c_str()), 1);
            std::cout << "->" << std::flush;
        }
    });

    std::thread h1s([&end, &h1_server] {
        char buf[1024];
        int counter = 0;

        h1_server->start_server(1234);
        while (true) {
            memset(buf, '\0', 1024);

            if (h1_server->recv(buf, 1024) > 0) {
                assert(buf[0] == counter * 2);
                std::cout << "<- " << std::flush;
            }

            counter++;

            if (end) break;
        }
    });

    std::thread h2s([&end, &h2_server, &h2_client] {
        char buf[1024];

        h2_server->start_server(1234);
        while (true) {
            memset(buf, '\0', 1024);

            // Listen for messages and send back 2 * the result
            if (h2_server->recv(buf, 1024) > 0) {
                char val = buf[0];
                h2_client->send("h1", 1234, const_cast<char*>(std::string(1, val * 2).c_str()), 1);
                std::cout << "=" << std::flush;
            }

            if (end) break;
        }
    });

    h1c.detach();
    h1s.detach();
    h2s.detach();

    std::this_thread::sleep_for(std::chrono::milliseconds(2100));

    end = true;
    h1_server->stop_server();
    h2_server->stop_server();

    std::cout << "]" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
});