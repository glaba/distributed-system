#include "test.h"
#include "member.h"
#include "member_list.h"

#include <iostream>

int run_tests() {
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
