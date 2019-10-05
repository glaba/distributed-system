#include "heartbeat.h"
#include "utils.h"

#include <chrono>
#include <iostream>
#include <unistd.h>

using namespace std::chrono;

heartbeater::heartbeater(member_list mem_list_, std::string local_hostname_, uint16_t port_) 
    : mem_list(mem_list_), local_hostname(local_hostname_), port(port_) {
    is_introducer = true;
}

heartbeater::heartbeater(member_list mem_list_, std::string local_hostname_, std::string introducer_, uint16_t port_) 
    : mem_list(mem_list_), local_hostname(local_hostname_), introducer(introducer_), port(port_) {
    is_introducer = false;
}

void heartbeater::start() {
    std::thread server_thread([this] {server();});
    server_thread.detach();

    client();
}

void heartbeater::join_group(std::string introducer) {
    // Send a message to the introducer saying that we want to join
    //  of the format J<hostname>\0<join_time>
    // J, \0 and join_time take 6 bytes, and the hostname is as long as it is
    char *join_msg = new char[6 + local_hostname.length()];
    join_msg[0] = 'J';
    for (unsigned i = 0; i < local_hostname.length(); i++) {
        join_msg[i + 1] = local_hostname[i];
    }
    join_msg[local_hostname.length() + 1] = '\0';
    *reinterpret_cast<uint32_t*>(join_msg + 2 + local_hostname.length()) = 
        htonl(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());

    // Create a UDP connection to the introducer
    udp_client_info conn = udp_client(introducer, std::to_string(port));

    // Send the message and close the connection
    sendto(conn.client_socket, join_msg, 6 + local_hostname.length(), 0, &conn.addr, sizeof(conn.addr));
    close(conn.client_socket);
}

bool heartbeater::has_joined() {
    return mem_list.num_members() > 0;
}

void heartbeater::client() {
    if (!is_introducer) {
        join_group(introducer);
    }

    // Remaining client code here
    while (true) {
        std::cout << "Just a client doing client things" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void heartbeater::server() {
    int server_fd = udp_server(port);

    // Remaining server code here
    while (true) {
        std::cout << "Just a server doing server things" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    close(server_fd);
}
