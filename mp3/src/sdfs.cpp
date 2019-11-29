#include "sdfs.h"

int main(int argc, char **argv) {
    std::string introducer = "";
    bool is_introducer = false;
    std::string local_hostname = "";
    uint16_t port = 1235; // 1235 will be port for heartbeater
    uint16_t el_port = 1236; // 1236 will be port for election protocol
    bool verbose = false;

    if (process_params(argc, argv, &introducer, &local_hostname, &verbose, &is_introducer)) {
        return 1;
    }

    // logger, udp interfaces, and mem list for heartbeater
    logger *lg_hb;// = new logger("", "heartbeater", verbose ? logger::log_level::level_trace : logger::log_level::level_info);
    udp_client_intf *udp_client_inst = new udp_client(lg_hb);
    udp_server_intf *udp_server_inst = new udp_server(lg_hb);
    member_list *mem_list = new member_list(local_hostname, lg_hb);

    heartbeater_intf *hb;
    if (introducer == "none") {
        hb = new heartbeater<true>(mem_list, lg_hb, udp_client_inst, udp_server_inst, local_hostname, port);
    } else {
        hb = new heartbeater<false>(mem_list, lg_hb, udp_client_inst, udp_server_inst, local_hostname, port);
    }

    logger *lg_el;// = new logger("election", verbose ? logger::log_level::level_trace : logger::log_level::level_info);
    udp_server_intf *election_udp_server_inst = new udp_server(lg_el);
    election *el = new election(hb, lg_el, udp_client_inst, election_udp_server_inst, el_port);

    el->start();
    hb->start();

    // tcp client and server
    // server is going to run on port 1237
    tcp_client client = tcp_client();
    tcp_server server = tcp_server();
    server.setup_server(1237);

    logger *lg_c;// = new logger("", verbose ? logger::log_level::level_trace : logger::log_level::level_info);
    sdfs_client *sdfsc = new sdfs_client(1237, client, lg_c, el, hb);

    logger *lg_s;// = new logger("sdfs_server.log", "", verbose ? logger::log_level::level_trace : logger::log_level::level_info);
    sdfs_server *sdfss = new sdfs_server(local_hostname, client, server, sdfsc, lg_s, hb, el);

    if (introducer != "none")
        hb->join_group(introducer);

    sdfss->start();
    sdfsc->start();

    /*
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    */
    return 0;
}
