#include "test.h"
#include "member.h"
#include "member_list.h"

#include <algorithm>
#include <cassert>
#include <iostream>

bool compare_hostnames(member &m1, member &m2) {
    return (m1.hostname.compare(m2.hostname) < 0) ? true : false;
}

int test_member_list() {
    std::cout << "=== TESTING INSERTING / READING MEMBER LIST WITH <5 MEMBERS ===" << std::endl;
    {
        member_list ml("local");
        std::vector<int> ids;
        ids.push_back(ml.add_member("1", 10));
        ids.push_back(ml.add_member("2", 10));
        ids.push_back(ml.add_member("local", 10));

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
            ids.push_back(ml.add_member(std::to_string(i), 10));
        }
        ids.push_back(ml.add_member("local", 10));

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
            ids.push_back(ml.add_member(std::to_string(i), 10));
        }
        ids.push_back(ml.add_member("9", 10));
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

int run_tests() {
    assert(test_member_list() == 0);

    /*
    int server_fd = udp_server(1234);
    socklen_t client_len;
    struct sockaddr_in client_sa;
    char buf[100];
    std::cout << "Server Is Awaiting Input " << buf << std::endl;
    int msg_size = recvfrom(server_fd, buf, 100, 0, (struct sockaddr *) &client_sa, &client_len);
    std::cout << "Server Received " << buf << std::endl;
    udp_client_info client_info = udp_client("127.0.0.1", "1234");

    char buf[100]; memset(buf, 0, 100);
    strcpy(buf, "test string\n");
    sendto(client_info.client_socket, buf, strlen(buf), MSG_CONFIRM, &client_info.addr, client_info.addr_len);

    // leaving this here temporarily to move some of it to other places later
    */
	std::cout << "=== ALL TESTS COMPLETED SUCCESSFULLY ===" << std::endl;
	return 0;
}
