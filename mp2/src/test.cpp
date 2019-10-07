#include "test.h"
#include "member.h"
#include "member_list.h"
#include "mock_udp.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

bool compare_hostnames(member &m1, member &m2) {
    return (m1.hostname.compare(m2.hostname) < 0) ? true : false;
}

int test_member_list() {
    std::cout << "=== TESTING INSERTING / READING MEMBER LIST WITH <5 MEMBERS ===" << std::endl;
    {
        member_list ml("local");
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
        member_list ml("local");
        std::vector<int> ids;
        for (int i = 0; i < 10; i++) {
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
        member_list ml("9");
        std::vector<int> ids;
        for (int i = 0; i < 9; i++) {
            ids.push_back(ml.add_member(std::to_string(i), std::hash<int>()(i)));
        }
        ids.push_back(ml.add_member("9", std::hash<int>()(9)));
        // ml: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

        for (int i = 0; i < 10; i += 2) {
            ml.remove_member(ids[i]);
        }
        // ml: 1, 3, 5, 7, 9

        std::list<member> list = ml.__get_internal_list();
        assert(list.size() == 5);

        // Check for sortedness
        uint32_t prev_id = 0;
        for (member m : list) {
            assert(m.id > prev_id);
            prev_id = m.id;
        }

        // Check that all the members that should be there are there
        list.sort(compare_hostnames);
        int i = 1;
        for (auto it = list.begin(); it != list.end(); it++) {
            assert(it->hostname == std::to_string(i));
            i += 2;
        }

        // Check that the neighbors are correct
        std::vector<member> neighbors = ml.get_neighbors();
        assert(neighbors.size() == 4);
    }

    return 0;
}

int test_mock_udp() {
    mock_udp_factory *fac = new mock_udp_factory();

    udp_client_svc *h1_client = fac->get_mock_udp_client("h1");
    udp_server_svc *h1_server = fac->get_mock_udp_server("h1");
    udp_client_svc *h2_client = fac->get_mock_udp_client("h2");
    udp_server_svc *h2_server = fac->get_mock_udp_server("h2");

    volatile bool end = false;

    std::thread h1c([h1_client] {
        for (char i = 0; i < 20; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            h1_client->send("h2", std::to_string(1234), const_cast<char*>(std::string(1, i).c_str()), 1);
        }

        std::cout << "h1c terminating" << std::endl;
    });

    std::thread h1s([end, h1_server] {
        char buf[1024];
        int counter = 0;

        while (true) {
            memset(buf, '\0', 1024);

            if (h1_server->recv(buf, 1024) > 0) {
                assert(buf[0] == counter * 2);
                std::cout << std::to_string(buf[0]) << std::endl;
            }

            counter++;

            if (end) break;
        }

        std::cout << "h1s terminating" << std::endl;
    });

    std::thread h2([end, h2_server, h2_client] {
        char buf[1024];

        while (true) {
            memset(buf, '\0', 1024);
            
            // Listen for messages and send back 2 * the result
            std::cout << "Listening" << std::endl;
            if (h2_server->recv(buf, 1024) > 0) {
                std::cout << "Got message" << std::endl;
                char val = buf[0];
                h2_client->send("h1", std::to_string(1234), const_cast<char*>(std::string(1, val * 2).c_str()), 1);
            }
            std::cout << "Past message" << std::endl;

            if (end) break;
        }

        std::cout << "h2s terminating" << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    std::cout << "Stopping servers" << std::endl;
    end = true;
    h1_server->stop_server();
    h2_server->stop_server();


    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    delete h1_client;
    delete h1_server;
    delete h2_client;
    delete h2_server;
    delete fac;
    return 0;
}

int run_tests() {
    assert(test_member_list() == 0);
    assert(test_mock_udp() == 0);

	std::cout << "=== ALL TESTS COMPLETED SUCCESSFULLY ===" << std::endl;
	return 0;
}
