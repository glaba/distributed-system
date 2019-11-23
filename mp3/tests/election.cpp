#include "test.h"
#include "election.h"
#include "heartbeater.h"
#include "mock_udp.h"
#include "environment.h"
#include "configuration.h"

#include <memory>
#include <unordered_map>
#include <random>

using std::unique_ptr;
using std::make_unique;

testing::register_test election_test("election.failover",
    "Tests that re-election of a master node after failure succeeds", [] (logger::log_level level)
{
    const int NUM_NODES = 10;

    environment_group env_group(true);
    std::vector<unique_ptr<environment>> envs = env_group.get_envs(NUM_NODES);

    // Set the hostnames and heartbeater / election parameters for each of the environments
    for (int i = 0; i < 10; i++) {
        configuration *config = envs[i]->get<configuration>();
        config->set_hostname("h" + std::to_string(i));
        config->set_hb_port(1234);
        config->set_first_node(i == 0);
        config->set_election_port(1235);

        envs[i]->get<logger_factory>()->configure(level);
    }

    std::vector<heartbeater*> hbs;
    for (int i = 0; i < NUM_NODES; i++) {
        hbs.push_back(envs[i]->get<heartbeater>());
    }
    hbs[0]->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<election*> elections;
    for (int i = 0; i < NUM_NODES; i++) {
        elections.push_back(envs[i]->get<election>());
        elections[i]->start();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    for (unsigned i = 1; i < NUM_NODES; i++) {
        hbs[i]->start();
        hbs[i]->join_group("h0");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(4000));

    // Assert that all nodes know who the master node is
    for (unsigned i = 0; i < NUM_NODES; i++) {
        elections[i]->get_master_node([&] (member master_node, bool succeeded) {
            assert(succeeded && "Node does not know who the master node is before failure");
            assert(master_node.id == hbs[0]->get_id() && "Node thinks wrong node is the master node");
        });
    }

    // Stop h0 and see how election proceeds
    elections[0]->stop();
    hbs[0]->stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(20000));

    // Assert that they successfully all re-elected some new member
    uint32_t new_master_id = 0;
    for (unsigned i = 1; i < NUM_NODES; i++) {
        elections[i]->get_master_node([&] (member new_master, bool succeeded) {
            assert(succeeded && "Node did not successfully re-elect a master node");
            if (new_master_id == 0) {
                new_master_id = new_master.id;
            } else {
                assert(new_master.id == new_master_id && "Split brain occurred after failure");
            }
        });
    }

    // Stop all heartbeaters
    for (unsigned i = 0; i < NUM_NODES; i++) {
        if (i == 0)
            continue;

        std::thread stop_thread([&hbs, &elections, i] {
            elections[i]->stop();
            hbs[i]->stop();
        });
        stop_thread.detach();
    }
    // Wait for them all to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(4500));
});

testing::register_test election_test_packet_loss("election.failover_packet_loss",
    "Tests that re-election of a master node after failure succeeds even with UDP packet loss", [] (logger::log_level level)
{
    const int NUM_NODES = 10;
    double drop_probability = 0.1;

    environment_group env_group(true);
    std::vector<unique_ptr<environment>> envs = env_group.get_envs(NUM_NODES);

    // Set the hostnames and heartbeater / election parameters for each of the environments
    for (int i = 0; i < 10; i++) {
        configuration *config = envs[i]->get<configuration>();
        config->set_hostname("h" + std::to_string(i));
        config->set_hb_port(1234);
        config->set_first_node(i == 0);
        config->set_election_port(1235);

        mock_udp_factory *fac = dynamic_cast<mock_udp_factory*>(envs[i]->get<udp_factory>());
        fac->configure(false, drop_probability);

        envs[i]->get<logger_factory>()->configure(level);
    }

    std::vector<heartbeater*> hbs;
    for (int i = 0; i < NUM_NODES; i++) {
        hbs.push_back(envs[i]->get<heartbeater>());
    }
    hbs[0]->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<election*> elections;
    for (int i = 0; i < NUM_NODES; i++) {
        elections.push_back(envs[i]->get<election>());
        elections[i]->start();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    for (unsigned i = 1; i < NUM_NODES; i++) {
        hbs[i]->start();
        hbs[i]->join_group("h0");
        hbs[i]->on_fail([] (member m) {
            if (m.hostname != "h0") {
                assert(false && "Heartbeater incorrectly lost a node due to packet loss. Retry test!");
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(4000));

    // Note down the IDs of all the nodes and map them to their index
    std::unordered_map<uint32_t, int> id_to_index;
    for (int i = 0; i < NUM_NODES; i++) {
        id_to_index[hbs[i]->get_id()] = i;
    }

    // Assert that all nodes know who the master node is
    for (unsigned i = 0; i < NUM_NODES; i++) {
        elections[i]->get_master_node([&] (member master_node, bool succeeded) {
            assert(succeeded && "Node does not know who the master node is before failure");
            assert(master_node.id == hbs[0]->get_id() && "Node thinks wrong node is the master node");
        });
    }

    // Stop h0 and see how election proceeds
    elections[0]->stop();
    hbs[0]->stop();

    // Wait for all nodes to detect failure
    std::this_thread::sleep_for(std::chrono::milliseconds(6000));

    // Wait for them to all re-elect some new member, and make sure a split brain never occurs
    while (true) {
        // Make sure that if anyone has decided on a master, EVERYONE who has decided agrees,
        //  even if not everyone has decided yet
        uint32_t new_master_id = 0;
        bool all_decided = true;
        for (unsigned i = 1; i < NUM_NODES; i++) {
            elections[i]->get_master_node([&] (member new_master, bool succeeded) {
                if (!succeeded) {
                    all_decided = false;
                    return;
                }

                if (new_master_id == 0) {
                    new_master_id = new_master.id;
                } else {
                    assert(new_master.id == new_master_id);
                }
            });
        }

        // If some nodes have decided but not all, make sure the supposed master node doesn't think anyone is master
        if (!all_decided && new_master_id != 0) {
            elections[id_to_index[new_master_id]]->get_master_node([&] (member master_node, bool succeeded) {
                assert(!succeeded && "Node incorrectly believes it is the master node");
            });
        }

        // If all nodes have decided and are in agreement, the test is complete
        if (all_decided) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop all heartbeaters
    for (unsigned i = 0; i < NUM_NODES; i++) {
        if (i == 0)
            continue;

        std::thread stop_thread([&hbs, &elections, i] {
            elections[i]->stop();
            hbs[i]->stop();
        });
        stop_thread.detach();
    }
    // Wait for them all to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(4500));
});