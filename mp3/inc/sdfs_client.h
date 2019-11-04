#pragma once

#include "tcp.h"
#include "election.h"
#include "heartbeater.h"

#include <sys/types.h>
#include <dirent.h>

#include <fstream>
#include <sstream>
#include <iterator>
#include <chrono>
#include <algorithm>

#define SDFS_DIR "/home/lawsonp2/.sdfs/"
#define SDFS_ACK_MSG "OK"
#define SDFS_SUCCESS_MSG "SUCCESS"
#define SDFS_FAILURE_MSG "FAILURE"

class sdfs_client {
public:
    sdfs_client(std::string protocol_port, tcp_client client, logger *lg, election *el, heartbeater_intf *hb) :
        protocol_port(protocol_port), client(client), lg(lg), el(el), hb(hb) {}

    void start();
    /*
     * main input loop for the sdfs client
     */
    void input_loop();
    /*
     * wrapper around the put operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    std::string put_operation_wr(std::string hostname, std::string local_filename, std::string sdfs_filename);
    /*
     * wrapper around the get operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    std::string get_operation_wr(std::string hostname, std::string local_filename, std::string sdfs_filename);
    /*
     * wrapper around the delete operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    std::string delete_operation_wr(std::string hostname, std::string sdfs_filename);
    /*
     * wrapper around the ls operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    std::string ls_operation_wr(std::string hostname, std::string sdfs_filename);
    /*
     * handles the put operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    std::string put_operation(std::string hostname, std::string local_filename, std::string sdfs_filename);
    /*
     * handles the get operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    std::string get_operation(std::string hostname, std::string local_filename, std::string sdfs_filename);
    /*
     * handles the delete operation with the given argument
     * returns 0 on success, -1 on failure
     **/
    std::string delete_operation(std::string hostname, std::string sdfs_filename);
    /*
     * handles the ls operation with the given argument
     * returns 0 on success, -1 on failure
     **/
    std::string ls_operation(std::string hostname, std::string sdfs_filename);
    /*
     * handles the relay put operation with the given argument
     * returns 0 on success, -1 on failure
     **/
    std::string relay_operation(std::string hostname, std::string relay_hostname, std::string operation);
    /*
     * handles the store operation
     * returns 0 on success, -1 on failure
     **/
    std::string store_operation();
private:
    std::vector<std::string> recv_mems_over_socket(int socket);
    int send_file_over_socket(int socket, std::string filename);
    int recv_file_over_socket(int socket, std::string filename);
    std::string get_file_location(std::vector<std::string> members, std::string filename);
    std::vector<member> get_file_destinations(std::string filename);

    std::string protocol_port;

    tcp_client client;
    logger *lg;
    election *el;
    heartbeater_intf *hb;
};
