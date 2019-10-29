#include "test.h"
#include "member.h"
#include "member_list.h"
#include "mock_udp.h"
#include "heartbeater.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

bool compare_hostnames(const member &m1, const member &m2) {
    return (m1.hostname.compare(m2.hostname) < 0) ? true : false;
}

int test_member_list(logger *lg) {
    std::cout << "=== TESTING INSERTING / READING MEMBER LIST WITH <5 MEMBERS ===" << std::endl;
    {
        member_list ml("local", lg);
        std::vector<int> ids;
        ids.push_back(ml.add_member("1", std::hash<std::string>()("1")));
        ids.push_back(ml.add_member("2", std::hash<std::string>()("2")));
        ids.push_back(ml.add_member("local", std::hash<std::string>()("local")));

        // Check that all the generated IDs are unique
        assert(std::unique(ids.begin(), ids.end()) == ids.end());

        std::vector<member> neighbors = ml.get_neighbors();
        assert(neighbors.size() == 2);

        std::sort(neighbors.begin(), neighbors.end(), compare_hostnames);
        assert(neighbors[0].hostname == "1");
        assert(neighbors[1].hostname == "2");
    }

    std::cout << "=== TESTING INSERTING / READING / SORTEDNESS OF MEMBER LIST WITH >5 MEMBERS ===" << std::endl;
    {
        member_list ml("local", lg);
        std::vector<int> ids;
        for (int i = 1; i < 11; i++) {
            ids.push_back(ml.add_member(std::to_string(i), std::hash<int>()(i)));
        }
        ids.push_back(ml.add_member("local", std::hash<std::string>()("local")));

        // Check that all the generated IDs are unique
        assert(std::unique(ids.begin(), ids.end()) == ids.end());

        std::vector<member> neighbors = ml.get_neighbors();
        assert(neighbors.size() == 4);

        assert(std::unique(neighbors.begin(), neighbors.end()) == neighbors.end());
        assert(std::find_if(neighbors.begin(), neighbors.end(), [](member m) {return m.hostname == "local";}) == neighbors.end());

        // Check that the internal list representation is correct
        std::list<member> list = ml.__get_internal_list();
        assert(list.size() == 11);

        // Check for sortedness
        uint32_t prev_id = 0;
        for (member m : list) {
            assert(m.id > prev_id);
            prev_id = m.id;
        }
    }

    std::cout << "=== TESTING REMOVING FROM MEMBER LIST ===" << std::endl;
    {
        member_list ml("8", lg);
        std::vector<int> ids;
        for (int i = 1; i < 8; i++) {
            ids.push_back(ml.add_member(std::to_string(i), std::hash<int>()(i)));
        }
        ids.push_back(ml.add_member("8", std::hash<int>()(8)));
        // ml: 1, 2, 3, 4, 5, 6, 7, 8

        for (int i = 0; i < 8; i += 2) {
            ml.remove_member(ids[i]);
        }
        // ml: 2, 4, 6, 8

        std::list<member> list = ml.__get_internal_list();
        assert(list.size() == 4);

        // Check for sortedness
        uint32_t prev_id = 0;
        for (member m : list) {
            assert(m.id > prev_id);
            prev_id = m.id;
        }
        // Check that all the members that should be there are there
        list.sort(compare_hostnames);
        int i = 2;
        for (auto it = list.begin(); it != list.end(); it++) {
            assert(it->hostname == std::to_string(i));
            i += 2;
        }

        // Check that the neighbors are correct
        std::vector<member> neighbors = ml.get_neighbors();
        assert(neighbors.size() == 3);
    }

    return 0;
}

int test_mock_udp(logger *lg) {
    std::cout << "=== TESTING CLIENT / SERVER COMMUNICATION WITH MOCK UDP SERVICES ===" << std::endl;

    mock_udp_factory *fac = new mock_udp_factory();

    udp_client_svc *h1_client = fac->get_mock_udp_client("h1", false);
    udp_server_svc *h1_server = fac->get_mock_udp_server("h1");
    udp_client_svc *h2_client = fac->get_mock_udp_client("h2", false);
    udp_server_svc *h2_server = fac->get_mock_udp_server("h2");

    volatile bool end = false;

    std::cout << "[ ";

    std::thread h1c([h1_client] {
        for (char i = 0; i < 20; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            h1_client->send("h2", std::to_string(1234), const_cast<char*>(std::string(1, i).c_str()), 1);
            std::cout << "->" << std::flush;
        }
    });

    std::thread h1s([&end, h1_server] {
        char buf[1024];
        int counter = 0;

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

    std::thread h2s([&end, h2_server, h2_client] {
        char buf[1024];

        while (true) {
            memset(buf, '\0', 1024);

            // Listen for messages and send back 2 * the result
            if (h2_server->recv(buf, 1024) > 0) {
                char val = buf[0];
                h2_client->send("h1", std::to_string(1234), const_cast<char*>(std::string(1, val * 2).c_str()), 1);
                std::cout << "=" << std::flush;
            }

            if (end) break;
        }
    });

    h1c.detach();
    h1s.detach();
    h2s.detach();

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    end = true;
    h1_server->stop_server();
    h2_server->stop_server();

    std::cout << "]" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    delete h1_client;
    delete h1_server;
    delete h2_client;
    delete h2_server;
    delete fac;
    return 0;
}

int test_joining(logger *lg) {
    std::cout << "=== TESTING JOINING ===" << std::endl;
    {
        const int NUM_NODES = 10;

        double drop_probability = 0.2;

        mock_udp_factory *fac = new mock_udp_factory();

        mock_udp_client_svc *clients[NUM_NODES];
        mock_udp_server_svc *servers[NUM_NODES];
        logger *loggers[NUM_NODES];
        member_list *mem_lists[NUM_NODES];

        for (int i = 0; i < NUM_NODES; i++) {
            clients[i] = fac->get_mock_udp_client("h" + std::to_string(i), false);
            clients[i]->set_drop_probability(drop_probability);
            servers[i] = fac->get_mock_udp_server("h" + std::to_string(i));
            if (lg->is_verbose()) {
                loggers[i] = new logger("h" + std::to_string(i), true);
            } else {
                loggers[i] = new logger("", "h" + std::to_string(i), false);
            }
            mem_lists[i] = new member_list("h" + std::to_string(i), loggers[i]);
        }

        // h1 is the introducer
        heartbeater_intf *hbs[NUM_NODES];
        hbs[0] = new heartbeater<true>(mem_lists[0], loggers[0], clients[0], servers[0], "h0", 1234);
        for (int i = 1; i < NUM_NODES; i++) {
            hbs[i] = new heartbeater<false>(mem_lists[i], loggers[i], clients[i], servers[i], "h" + std::to_string(i), 1234);
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
            std::thread stop_thread([hbs, i] {
                hbs[i]->stop();
            });
            stop_thread.detach();
        }
        // Wait for them all to stop
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    return 0;
}

int run_tests(logger *lg) {
    assert(test_member_list(lg) == 0);
    assert(test_mock_udp(lg) == 0);
    assert(test_joining(lg) == 0);

	std::cout << "=== ALL TESTS COMPLETED SUCCESSFULLY ===" << std::endl;
	return 0;
}
