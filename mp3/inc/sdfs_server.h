#include "tcp.h"

#include <sys/stat.h>

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

    int send_file_over_socket(int socket, std::string filename);
    int recv_file_over_socket(int socket, std::string filename);

    tcp_server server;
};
