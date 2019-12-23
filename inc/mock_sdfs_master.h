#pragma once

#include "sdfs_master.h"
#include "sdfs_client.h"
#include "mock_sdfs_client.h"
#include "environment.h"

#include <vector>
#include <string>

class mock_sdfs_master : public sdfs_master, public service_impl<mock_sdfs_master> {
public:
    mock_sdfs_master(environment &env);

    void start() {}
    void stop() {}
    // handles a put request over the specified socket
    int put_operation(int socket, std::string sdfs_filename) {return -1;}
    // handles a get request over the specified socket
    int get_operation(int socket, std::string sdfs_filename) {return -1;}
    // handles a del request over the specified socket
    int del_operation(int socket, std::string sdfs_filename) {return -1;}
    // handles a ls request over the specified socket
    int ls_operation(int socket, std::string sdfs_filename) {return -1;}
    // handles a append request over the specified socket
    int append_operation(int socket, std::string metadata, std::string sdfs_filename) {return -1;}
    // callback for append request
    void on_append(std::function<void(std::string filename, int offset, std::string metadata)> callback) {}
    std::vector<std::string> get_files_by_prefix(std::string prefix);
    int get_index_operation(int socket, std::string sdfs_filename) {return -1;};

private:
    mock_sdfs_client *client;
};
