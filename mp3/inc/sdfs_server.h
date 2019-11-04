#include "tcp.h"
#include "election.h"

#include <sys/stat.h>

#include <fstream>
#include <sstream>
#include <iterator>

#define SDFS_DIR "/home/lawsonp2/.sdfs/"
#define SDFS_ACK_MSG "OK"
#define SDFS_SUCCESS_MSG "SUCCESS"
#define SDFS_FAILURE_MSG "FAILURE"

class sdfs_server {
public:
    sdfs_server(tcp_server server, logger *lg, heartbeater_intf *hb, election *el) :
        server(server), lg(lg), hb(hb), el(el) {}
    void process_client();

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

    int send_file_over_socket(int socket, std::string filename);
    int recv_file_over_socket(int socket, std::string filename);

    tcp_server server;
    logger *lg;
    heartbeater_intf *hb;
    election *el;
};
