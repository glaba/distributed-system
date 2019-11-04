#include "sdfs.h"

void sdfs_client_start(sdfs_client client) {
    // client.put_operation("test.txt", "test2.txt");
    client.input_loop();
    return;
}

void sdfs_server_start(sdfs_server server) {
    server.process_client();
    return;
}

int process_params(int argc, char **argv, std::string *introducer, std::string *local_hostname,
        bool *verbose, bool *is_introducer);

int main(int argc, char **argv) {
    std::string introducer = "";
    bool is_introducer = false;
    std::string local_hostname = "";
    uint16_t port = 1235; // 1235 will be port for heartbeater
    bool verbose = false;

    if (process_params(argc, argv, &introducer, &local_hostname, &verbose, &is_introducer)) {
        return 1;
    }

    // logger, udp interfaces, and mem list for heartbeater
    logger *lg = new logger("", verbose);
    udp_client_intf *udp_client_inst = new udp_client(lg);
    udp_server_intf *udp_server_inst = new udp_server(lg);
    member_list *mem_list = new member_list(local_hostname, lg);

    heartbeater_intf *hb;
    if (introducer == "none") {
        hb = new heartbeater<true>(mem_list, lg, udp_client_inst, udp_server_inst, local_hostname, port);
    } else {
        hb = new heartbeater<false>(mem_list, lg, udp_client_inst, udp_server_inst, local_hostname, port);
    }

    hb->start();

    if (introducer != "none") hb->join_group(introducer);

    // tcp client and server
    // server is going to run on port 1237
    tcp_client client = tcp_client();
    tcp_server server = tcp_server("1237");

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return 0;
}

void print_help() {
    std::cout << "Usage: member [-i <introducer> -h <hostname>] -v" << std::endl << std::endl;
    std::cout << "Option\tMeaning" << std::endl;
    std::cout << " -h\tThe hostname of this machine that other members can use" << std::endl;
    std::cout << " -i\tIntroducer hostname or \"none\" if this is the first introducer" << std::endl;
    std::cout << " -n\tThis machine is an introducer" << std::endl;
    std::cout << " -v\tEnable verbose logging" << std::endl;
}

int print_invalid() {
    std::cout << "Invalid arguments" << std::endl;
    print_help();
    return 1;
}

int process_params(int argc, char **argv, std::string *introducer, std::string *local_hostname,
    bool *verbose, bool *is_introducer) {
    if (argc == 1)
        return print_invalid();

    for (int i = 1; i < argc; i++) {
        // The introducer
        if (std::string(argv[i]) == "-i") {
            if (i + 1 < argc) {
                *introducer = std::string(argv[i + 1]);
            } else return print_invalid();
            i++;
        // The hostname
        } else if (std::string(argv[i]) == "-h") {
            if (i + 1 < argc) {
                *local_hostname = std::string(argv[i + 1]);
            }
            i++;
        // Whether or not to use verbose logging
        } else if (std::string(argv[i]) == "-v") {
            *verbose = true;
        } else if (std::string(argv[i]) == "-n") {
            *is_introducer = true;
        } else return print_invalid();
    }

    if (*introducer == "") {
        std::cout << "Option -i required" << std::endl;
        return 1;
    }

    if (*local_hostname == "") {
        std::cout << "Option -h required" << std::endl;
        return 1;
    }

    return 0;
}

/*
void client_start(tcp_client client) {
    // int setup_connection(std::string host, std::string port);
    // std::string read_from_server(int socket);
    // ssize_t write_to_server(int socket, std::string data);
    // void close_connection(int socket);

    int fd = client.setup_connection("127.0.0.1", "1235");

    std::string sent = "a message for t2";
    client.write_to_server(fd, sent);
    std::cout << "t1 sent " << sent << std::endl;

    std::string recvd = client.read_from_server(fd);
    std::cout << "t1 recvd as a response " << recvd << std::endl;

    client.close_connection(fd);
}

void server_start(tcp_server server) {
    // int accept_connection();
    // std::string read_from_client(int client);
    // ssize_t write_to_client(int client, std::string data);
    // void close_connection(int client_socket);
    // void tear_down_server();


    int client = server.accept_connection();

    std::string recvd = server.read_from_client(client);
    std::cout << "t2 recvd " << recvd << std::endl;

    std::string sent = "a message for t1";
    server.write_to_client(client, sent);
    std::cout << "t2 replied with " << sent << std::endl;

    server.close_connection(client);
}

    // main function needs to kick off the server thread that will
    // run the main server logic - an interrupt handler needs to remove all the files on exit btw

    // then the main thread will be running the client input loop that will be the main client interface

    tcp_client client = tcp_client();
    tcp_server server = tcp_server("1235");

    // std::thread t1(client_start, client);
    // std::thread t2(server_start, server);

    // t1.join();
    // t2.join();

    uint16_t port = 1237;
    uint16_t el_port = 1238;

    logger *lg1 = new logger("", false);
    member_list *mem_list1 = new member_list("127.0.0.1", lg1);

    udp_client_intf *udp_client_inst1 = new udp_client(lg1);
    udp_server_intf *udp_server_inst1 = new udp_server(lg1);

    heartbeater_intf *hb1 = new heartbeater<true>(mem_list1, lg1, udp_client_inst1, udp_server_inst1, "127.0.0.1", port);
    election *el1 = new election(hb1, lg1, udp_client_inst1, udp_server_inst1, el_port);
    sdfs_server sdfss = sdfs_server("127.0.0.1", client, server, lg1, hb1, el1);

    // hb = new heartbeater<false>(mem_list, lg, udp_client_inst, udp_server_inst, local_hostname, port);
    // sdfs_client(std::string master_hostname, std::string fs_port, tcp_client client, logger *lg, election *el) :
        // master_hostname(master_hostname), fs_port(fs_port), client(client), lg(lg), el(el) {}

    logger *lg2 = new logger("", false);
    member_list *mem_list2 = new member_list("127.0.0.1", lg1);

    udp_client_intf *udp_client_inst2 = new udp_client(lg2);
    udp_server_intf *udp_server_inst2 = new udp_server(lg2);

    heartbeater_intf *hb2 = new heartbeater<false>(mem_list2, lg2, udp_client_inst2, udp_server_inst2, "127.0.0.1", port);
    election *el2 = new election(hb2, lg2, udp_client_inst2, udp_server_inst2, el_port);
    sdfs_client sdfsc = sdfs_client("1235", client, lg2, el2);

    // sdfs_server(tcp_server server, logger *lg, heartbeater_intf *hb, election *el) :
        // server(server), lg(lg), hb(hb), el(el) {}

    std::thread t3(sdfs_server_start, sdfss);
    // std::thread t4(sdfs_client_start, sdfsc);
    sdfs_client_start(sdfsc);
    t3.join();
    // t4.join();
*/
