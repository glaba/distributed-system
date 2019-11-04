#include "tcp.h"
#include "election.h"
#include <fstream>
#include <sstream>

#define SDFS_ACK_MSG "OK"
#define SDFS_SUCCESS_MSG "SUCCESS"
#define SDFS_FAILURE_MSG "FAILURE"

class sdfs_client {
public:
    sdfs_client(std::string master_hostname, std::string fs_port, tcp_client client, logger *lg, election *el) :
        master_hostname(master_hostname), fs_port(fs_port), client(client), lg(lg), el(el) {}
    /*
     * handles the put operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    std::string put_operation(std::string local_filename, std::string sdfs_filename);
    /*
     * handles the get operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    std::string get_operation(std::string local_filename, std::string sdfs_filename);
    /*
     * handles the delete operation with the given argument
     * returns 0 on success, -1 on failure
     **/
    std::string delete_operation(std::string sdfs_filename);
    /*
     * handles the ls operation with the given argument
     * returns 0 on success, -1 on failure
     **/
    std::string ls_operation(std::string sdfs_filename);
    /*
     * handles the store operation
     * returns 0 on success, -1 on failure
     **/
    std::string store_operation();
private:
    int send_file_over_socket(int socket, std::string filename);
    int recv_file_over_socket(int socket, std::string filename);

    std::string master_hostname;
    std::string fs_port;

    tcp_client client;
    logger *lg;
    election *el;
};
