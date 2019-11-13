#include "test.h"
#include "heartbeater.h"
#include "mock_udp.h"

#include <memory>
#include <set>

using std::unique_ptr;
using std::make_unique;

testing::register_test joining_group("heartbeater.joining_group",
    "Tests that 10 nodes can successfully join and remain in the group", [] (logger *lg)
{
    const int NUM_NODES = 10;

    double drop_probability = 0.2;

    unique_ptr<mock_udp_factory> fac = make_unique<mock_udp_factory>();

    unique_ptr<udp_client_intf> clients[NUM_NODES];
    unique_ptr<udp_server_intf> servers[NUM_NODES];
    unique_ptr<logger> loggers[NUM_NODES];
    unique_ptr<member_list> mem_lists[NUM_NODES];

    for (int i = 0; i < NUM_NODES; i++) {
        clients[i] = fac->get_mock_udp_client("h" + std::to_string(i), false, drop_probability);
        servers[i] = fac->get_mock_udp_server("h" + std::to_string(i));
        if (lg->is_verbose()) {
            loggers[i] = make_unique<logger>("h" + std::to_string(i), true);
        } else {
            loggers[i] = make_unique<logger>("", "h" + std::to_string(i), false);
        }
        mem_lists[i] = make_unique<member_list>("h" + std::to_string(i), loggers[i].get());
    }

    // h1 is the introducer
    unique_ptr<heartbeater_intf> hbs[NUM_NODES];
    hbs[0] = make_unique<heartbeater<true>>(mem_lists[0].get(), loggers[0].get(), clients[0].get(), servers[0].get(), "h0", 1234);
    for (int i = 1; i < NUM_NODES; i++) {
        hbs[i] = make_unique<heartbeater<false>>(mem_lists[i].get(), loggers[i].get(), clients[i].get(), servers[i].get(), "h" + std::to_string(i), 1234);
    }

    hbs[0]->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    for (unsigned i = 1; i < NUM_NODES; i++) {
        // Ensure that all the membership lists of the previous nodes are correct
        for (unsigned j = 0; j < i; j++) {
            std::vector<member> members = hbs[j]->get_members();
            // Ensure that the right number of members is there
            assert(members.size() == i && "Change drop_probability to 0 to confirm test failure");

            // Ensure that the members that are there are correct
            std::set<std::string> ids;
            for (auto m : members) {
                ids.insert(m.hostname);
            }
            for (unsigned k = 0; k < i - 1; k++) {
                assert(ids.find("h" + std::to_string(k)) != ids.end());
            }
        }

        hbs[i]->start();
        hbs[i]->join_group("h0");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    // Check that all the nodes stay connected for the next 5 seconds
    for (unsigned i = 0; i < 5; i++) {
        for (unsigned j = 0; j < NUM_NODES; j++) {
            std::vector<member> members = hbs[j]->get_members();

            // Same checks as before
            assert(members.size() == NUM_NODES && "Change drop_probability to 0 to confirm test failure");
            std::set<std::string> ids;
            for (auto m : members) {
                ids.insert(m.hostname);
            }
            for (unsigned k = 0; k < NUM_NODES; k++) {
                assert(ids.find("h" + std::to_string(k)) != ids.end());
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Stop all heartbeaters
    for (unsigned i = 0; i < NUM_NODES; i++) {
        std::thread stop_thread([&hbs, i] {
            hbs[i]->stop();
        });
        stop_thread.detach();
    }
    // Wait for them all to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
});
