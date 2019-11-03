#include "tcp.h"

#define SDFS_DIR "~/.sdfs"
#define SDFS_ACK_MSG "OK"
#define SDFS_SUCCESS_MSG "SUCCESS"
#define SDFS_FAILURE_MSG "FAILURE"

class sdfs_server {
public:
    sdfs_server(tcp_client server) : server(server) {}
    void process_client(int client);

private:
    /*
     * handles put request
     * returns SDFS_SUCCESS_MSG on success, SDFS_FAILURE_MSG on failure
     **/
    std::string put_operation(std::string filename);
    /*
     * handles get request
     * returns SDFS_SUCCESS_MSG on success, SDFS_FAILURE_MSG on failure
     **/
    std::string get_operation(std::string filename);
    /*
     * handles delete request
     * returns SDFS_SUCCESS_MSG on success, SDFS_FAILURE_MSG on failure
     **/
    std::string delete_operation(std::string filename);
    /*
     * handles ls request
     * returns SDFS_SUCCESS_MSG on success, SDFS_FAILURE_MSG on failure
     **/
    std::string ls_operation(std::string filename);

    tcp_server server;
};
