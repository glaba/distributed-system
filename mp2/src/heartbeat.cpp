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
    struct timeval tv; // i'm currently experiencing with simply setting a timeout on the socket
    tv.tv_sec = 0;     // typically you would use select or epoll, but our use case seems slightly different
    tv.tv_usec = 5000; // than the typical select/epoll use case
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
            perror("Error");
    }

    int msg_size;
    char buf[1024];

    socklen_t client_len;
    struct sockaddr_in client_sa;

    // code to listen for messages and handle them accordingly here
    while (true) {
        // std::cout << "Just a server doing server things" << std::endl;
        std::cout << "listening for heartbeats" << std::endl;
        // listen for messages and process, otherwise sleep until the next cycle
        while ((msg_size = recvfrom(server_fd, buf, 1024, 0, (struct sockaddr *) &client_sa, &client_len)) > 0) {
            if (buf[0] == 'T') {
                continue;
                process_table_msg(buf);
            }
            else if (buf[0] == 'J') {
                continue;
                process_join_msg(buf);
            }
            else if (buf[0] == 'L') {
                continue;
                process_leave_msg(buf);
            }
            // update last heartbeat
            memset(buf, 0, 1024);   // most certainly a better way to do this btw
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    close(server_fd);
}

void heartbeater::process_table_msg(std::string msg) {
    // table message is of the form "T<hostname>\0<id>\0<hostname>\0<id>..."
    // each member has a hostname and an id
    std::cout << "processing table message : " + msg << std::endl;
    // method of processing inspired by https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
    unsigned int start = 1;
    unsigned int end = msg.find("\0");
    while (end != std::string::npos) {
        // get the hostname
        std::string hostname = msg.substr(start, end - start);
        std::cout << "hostname is : " + hostname << std::endl;

        // get the id
        start = end + 1;
        end = msg.find("\0", start);
        int id = std::stoi(msg.substr(start, end - start));
        std::cout << "id is : " + id << std::endl;

        // add to the member list
        mem_list.add_member(hostname, id);

        // iterate to next hostname & id
        start = end + 1;
        end = msg.find("\0", start);
    }
    std::cout << "finised processing table message" << std::endl;
}

void heartbeater::process_join_msg(std::string msg) {
    // join meessage is of the form "J<hostname>\0<join_time>"
    std::cout << "processing join message : " + msg << std::endl;
    unsigned int start = 1;
    unsigned int end = msg.find("\0");
    std::string hostname = msg.substr(start, end - start);
    std::cout << "hostname is : " + hostname << std::endl;

    // get the id
    start = end + 1;
    end = msg.length();
    int join_time = std::stoi(msg.substr(start, end - start));
    std::cout << "join_time is : " + join_time << std::endl;
    std::cout << "finised processing join message" << std::endl;
}

void heartbeater::process_leave_msg(std::string msg) {
    // leave message is of the form "L<hostname>\0"
    std::cout << "processing leave message : " + msg << std::endl;
    unsigned int start = 1;
    unsigned int end = msg.find("\0");
    std::string hostname = msg.substr(start, end - start);
    mem_list.remove_member_by_hostname(hostname);
    std::cout << "finised processing leave message" << std::endl;
}

/*
typedef struct member {
    uint32_t id;
    std::string hostname;
    uint64_t last_heartbeat;
    bool operator==(const member &m);
} member;
*/
// TODO: are we okay with just adding these in using hostname and ID?
// if not, we need to modify member to also include a join time and
// the TODO refers to changing this implementation to use join_time instead
std::string heartbeater::construct_table_msg() {
    // table message is of the form "T<hostname>\0<id>\0<hostname>\0<id>..."
    std::string result = "T";
    std::list<member> members = mem_list.__get_internal_list();
    for (auto mem : members) {
        result += (mem.hostname + "\0" + std::to_string(mem.id) + "\0");
    }
    return result;
}

std::string heartbeater::construct_leave_msg(std::string hostname) {
    std::string result = "L" + hostname + "\0";
    return result;
}

std::string heartbeater::construct_fail_msg(std::string hostname) {
    std::string result = "F" + hostname + "\0";
    return result;
}
