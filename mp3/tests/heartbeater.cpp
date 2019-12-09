#include "test.h"
#include "heartbeater.h"
#include "mock_udp.h"
#include "environment.h"
#include "configuration.h"
#include "logging.h"

#include <memory>
#include <set>
#include <functional>
#include <random>

using std::unique_ptr;
using std::make_unique;

template <bool UseRandomIntroducers>
std::function<void(logger::log_level)> test_fn([] (logger::log_level level) {
    const int NUM_NODES = 10;
    bool show_packets = false;
    double drop_probability = 0.1;

    environment_group env_group(true);
    std::vector<unique_ptr<environment>> envs = env_group.get_envs(NUM_NODES);

    // Set the hostnames and heartbeater parameters for each of the environments
    // Also set the mock UDP drop probability
    for (int i = 0; i < NUM_NODES; i++) {
        configuration *config = envs[i]->get<configuration>();
        config->set_hostname("h" + std::to_string(i));
        config->set_hb_port(1234);
        config->set_first_node(i == 0);

        mock_udp_factory *fac = dynamic_cast<mock_udp_factory*>(envs[i]->get<udp_factory>());
        fac->configure(show_packets, drop_probability);

        envs[i]->get<logger_factory>()->configure(level);
    }

    std::vector<heartbeater*> hbs;
    for (int i = 0; i < NUM_NODES; i++) {
        hbs.push_back(envs[i]->get<heartbeater>());
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
        if (UseRandomIntroducers) {
            hbs[i]->join_group("h" + std::to_string(std::rand() % i));
        } else {
            hbs[i]->join_group("h0");
        }
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

testing::register_test joining_group("heartbeater.joining_group",
    "Tests that 10 nodes can successfully join and remain in the group",
    30,
    test_fn<false>);

testing::register_test joining_group_random_introducer("heartbeater.random_introducer",
    "Tests that 10 nodes can successfully join and remain in the group using any node as the introducer",
    30,
    test_fn<true>);
