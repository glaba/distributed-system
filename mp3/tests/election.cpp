#include "test.h"
#include "election.h"
#include "heartbeater.h"
#include "mock_udp.h"

#include <memory>
#include <unordered_map>

using std::unique_ptr;
using std::make_unique;

testing::register_test election_test("election.failover",
    "Tests that re-election of a master node after failure succeeds", [] (logger *lg)
{
    const int NUM_NODES = 10;

    double drop_probability = 0.0;

    unique_ptr<mock_udp_factory> fac = make_unique<mock_udp_factory>();

    unique_ptr<udp_client_intf> clients[NUM_NODES];
    unique_ptr<udp_server_intf> hb_servers[NUM_NODES];
    unique_ptr<udp_server_intf> election_servers[NUM_NODES];
    unique_ptr<logger> loggers[NUM_NODES];
    unique_ptr<member_list> mem_lists[NUM_NODES];
    unique_ptr<election> elections[NUM_NODES];

    for (int i = 0; i < NUM_NODES; i++) {
        clients[i] = fac->get_mock_udp_client("h" + std::to_string(i), false, drop_probability);
        hb_servers[i] = fac->get_mock_udp_server("h" + std::to_string(i));
        election_servers[i] = fac->get_mock_udp_server("h" + std::to_string(i));
        loggers[i] = make_unique<logger>("h" + std::to_string(i), *lg);
        mem_lists[i] = make_unique<member_list>("h" + std::to_string(i), loggers[i].get());
    }

    // h0 is the introducer
    unique_ptr<heartbeater_intf> hbs[NUM_NODES];
    hbs[0] = make_unique<heartbeater<true>>(mem_lists[0].get(), loggers[0].get(), clients[0].get(), hb_servers[0].get(), "h0", 1234);
    for (int i = 1; i < NUM_NODES; i++) {
        hbs[i] = make_unique<heartbeater<false>>(mem_lists[i].get(), loggers[i].get(), clients[i].get(), hb_servers[i].get(), "h" + std::to_string(i), 1234);
    }

    hbs[0]->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    for (int i = 0; i < NUM_NODES; i++) {
        elections[i] = make_unique<election>(hbs[i].get(), loggers[i].get(), clients[i].get(), election_servers[i].get(), 1235);
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
        bool succeeded;
        member master_node = elections[i]->get_master_node(&succeeded);

        assert(succeeded && "Node does not know who the master node is before failure");
        assert(master_node.id == hbs[0]->get_id() && "Node thinks wrong node is the master node");
    }

    // Stop h0 and see how election proceeds
    elections[0]->stop();
    hbs[0]->stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(20000));

    // Assert that they successfully all re-elected some new member
    uint32_t new_master_id = 0;
    for (unsigned i = 1; i < NUM_NODES; i++) {
        bool succeeded;
        member new_master = elections[i]->get_master_node(&succeeded);

        assert(succeeded && "Node did not successfully re-elect a master node");
        if (new_master_id == 0) {
            new_master_id = new_master.id;
        } else {
            assert(new_master.id == new_master_id && "Split brain occurred after failure");
        }
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
    "Tests that re-election of a master node after failure succeeds even with UDP packet loss", [] (logger *lg)
{
    const int NUM_NODES = 10;

    double drop_probability = 0.25;

    unique_ptr<mock_udp_factory> fac = make_unique<mock_udp_factory>();

    unique_ptr<udp_client_intf> clients[NUM_NODES];
    unique_ptr<udp_server_intf> hb_servers[NUM_NODES];
    unique_ptr<udp_server_intf> election_servers[NUM_NODES];
    unique_ptr<logger> loggers[NUM_NODES];
    unique_ptr<member_list> mem_lists[NUM_NODES];
    unique_ptr<election> elections[NUM_NODES];

    for (int i = 0; i < NUM_NODES; i++) {
        clients[i] = fac->get_mock_udp_client("h" + std::to_string(i), false, drop_probability);
        hb_servers[i] = fac->get_mock_udp_server("h" + std::to_string(i));
        election_servers[i] = fac->get_mock_udp_server("h" + std::to_string(i));
        loggers[i] = make_unique<logger>("h" + std::to_string(i), *lg);
        mem_lists[i] = make_unique<member_list>("h" + std::to_string(i), loggers[i].get());
    }

    // h0 is the introducer
    unique_ptr<heartbeater_intf> hbs[NUM_NODES];
    hbs[0] = make_unique<heartbeater<true>>(mem_lists[0].get(), loggers[0].get(), clients[0].get(), hb_servers[0].get(), "h0", 1234);
    for (int i = 1; i < NUM_NODES; i++) {
        hbs[i] = make_unique<heartbeater<false>>(mem_lists[i].get(), loggers[i].get(), clients[i].get(), hb_servers[i].get(), "h" + std::to_string(i), 1234);
    }

    hbs[0]->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    for (int i = 0; i < NUM_NODES; i++) {
        elections[i] = make_unique<election>(hbs[i].get(), loggers[i].get(), clients[i].get(), election_servers[i].get(), 1235);
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
        bool succeeded;
        member master_node = elections[i]->get_master_node(&succeeded);

        assert(succeeded && "Node does not know who the master node is before failure");
        assert(master_node.id == hbs[0]->get_id() && "Node thinks wrong node is the master node");
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
            bool succeeded;
            member new_master = elections[i]->get_master_node(&succeeded);

            if (!succeeded) {
                all_decided = false;
                continue;
            }

            if (new_master_id == 0) {
                new_master_id = new_master.id;
            } else {
                assert(new_master.id == new_master_id);
            }
        }

        // If some nodes have decided but not all, make sure the supposed master node doesn't think anyone is master
        if (!all_decided && new_master_id != 0) {
            bool succeeded;
            elections[id_to_index[new_master_id]]->get_master_node(&succeeded);

            assert(!succeeded);
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