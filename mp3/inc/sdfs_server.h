#include "tcp.h"

#include <fstream>
#include <sstream>
#include <iterator>

#define SDFS_DIR "~/.sdfs"
#define SDFS_ACK_MSG "OK"
#define SDFS_SUCCESS_MSG "SUCCESS"
#define SDFS_FAILURE_MSG "FAILURE"

class sdfs_server {
public:
    sdfs_server(tcp_server server) : server(server) {}
    void process_client(int client);

private:
    /*
     * handles put request
     * returns SDFS_SUCCESS_MSG on success, SDFS_FAILURE_MSG on failure
     **/
    int put_operation(int client, std::string filename);
    /*
     * handles get request
     * returns SDFS_SUCCESS_MSG on success, SDFS_FAILURE_MSG on failure
     **/
    int get_operation(int client, std::string filename);
    /*
     * handles delete request
     * returns SDFS_SUCCESS_MSG on success, SDFS_FAILURE_MSG on failure
     **/
    int delete_operation(int client, std::string filename);

    /* ls operation will be handled by the master node, naturally */

    int send_file_over_socket(int socket, std::string filename);
    int recv_file_over_socket(int socket, std::string filename);

    tcp_server server;
};
