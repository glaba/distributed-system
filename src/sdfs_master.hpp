// #pragma once

// #include "sdfs_master.h"
// #include "sdfs_message.h"
// #include "sdfs_utils.hpp"
// #include "member_list.h"
// #include "heartbeater.h"
// #include "election.h"
// #include "logging.h"
// #include "tcp.h"
// #include "service.h"
// #include "environment.h"

// #include <mutex>
// #include <cstring>
// #include <sstream>
// #include <algorithm>
// #include <unordered_map>

// #define NUM_REPLICAS 4

// class sdfs_master_impl : public sdfs_master, public service_impl<sdfs_master_impl> {
// public:
//     sdfs_master_impl(environment &env);

//     void start();
//     void stop();
//     std::vector<std::string> get_files_by_prefix(std::string prefix);

//     // Functions to handle major sdfs operations as the master
//     int put_operation(int socket, std::string sdfs_filename);
//     int get_operation(int socket, std::string sdfs_filename);
//     int del_operation(int socket, std::string sdfs_filename);
//     int ls_operation(int socket, std::string sdfs_filename);
//     int append_operation(int socket, std::string metadata, std::string sdfs_filename);
//     int get_index_operation(int socket, std::string sdfs_filename);

//     void on_append(std::function<void(std::string filename, int offset, std::string metadata)> callback);

// private:
//     void process_loop();
//     void handle_connection(int socket);

//     // used to replicate a given file
//     int rep_operation(tcp_client *client, std::string hostname, std::string sdfs_filename);
//     // used to receive a list of files over a socket
//     int files_operation(int socket, std::string hostname, std::string data);

//     // callback for when a given member fails
//     void handle_failures(member failed_node);

//     bool sdfs_file_exists(std::string sdfs_filename);
//     std::vector<std::string> get_hostnames();

//     int get_num_shards_by_prefix(std::string prefix);
//     std::string get_next_filename_by_prefix(std::string prefix);

//     // thread safe add and remove
//     void add_to_maps(std::string hostname, std::string filename);
//     std::string get_hostname_from_map(std::string filename, int idx);

//     // Services that we depend on
//     election *el;
//     heartbeater *hb;
//     std::unique_ptr<logger> lg;
//     tcp_factory *fac;
//     std::unique_ptr<tcp_server> server;
//     std::unordered_map<std::string, std::vector<std::string>> file_to_hostnames;
//     std::unordered_map<std::string, std::vector<std::string>> hostname_to_files;
//     std::vector<std::function<void(std::string filename, int offset, std::string metadata)>> append_callbacks;

//     // lock
//     std::recursive_mutex maps_mtx;
//     configuration *config;
// };
