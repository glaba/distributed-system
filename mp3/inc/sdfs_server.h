#pragma once

#include "tcp.h"
#include "sdfs_client.h"
#include "election.h"

#include <sys/stat.h>

#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <map>

#define SDFS_DIR "/home/lawsonp2/.sdfs/"
#define SDFS_ACK_MSG "OK"
#define SDFS_SUCCESS_MSG "SUCCESS"
#define SDFS_FAILURE_MSG "FAILURE"

class sdfs_server {
public:
    sdfs_server(std::string hostname, tcp_client client, tcp_server server, logger *lg, heartbeater_intf *hb, election *el) :
        hostname(hostname), client(client), server(server), lg(lg), hb(hb), el(el) {}
    void start();
    void process_client();

private:
    void process_loop();
    /*
     * handles put request
     * returns 0 on success
     **/
    int put_operation(int client, std::string filename);
    /*
     * handles get request
     * returns 0 on success
     **/
    int get_operation(int client, std::string filename);
    /*
     * handles delete request
     * returns 0 on success
     **/
    int delete_operation(int client, std::string filename);
    /*
     * handles ls request
     * returns 0 on success
     **/
    int ls_operation(int client, std::string filename);


    /*
     * handles put request as master node
     * returns 0 on success
     **/
    int put_operation_mn(int client, std::string filename);
    /*
     * handles get request as master node
     * returns 0 on success
     **/
    int get_operation_mn(int client, std::string filename);
    /*
     * handles delete request as master node
     * returns 0 on success
     **/
    int delete_operation_mn(int client, std::string filename);
    /*
     * handles ls request as master node
     * returns 0 on success
     **/
    int ls_operation_mn(int client, std::string filename);
    /*
     * fixes replication scheme on member fail
     **/
    void fix_replicas(member failed_member);
    /*
     * sends a vector to the client
     **/
    void send_client_mem_vector(int client, std::vector<member> vec);
    /*
     * hashes a given filename to get min(3, members.size) members to store the file)
     * returns vector of members
     **/
    std::vector<member> get_file_destinations(std::string filename);

    int send_file_over_socket(int socket, std::string filename);
    int recv_file_over_socket(int socket, std::string filename);

    std::string hostname;

    tcp_client client;
    tcp_server server;

    sdfs_client *sdfsc;
    logger *lg;
    heartbeater_intf *hb;
    election *el;

    // map of node ids to vector of names
    std::map<uint32_t, std::vector<std::string>> ids_to_files;
};
