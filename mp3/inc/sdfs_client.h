#include "tcp.h"

class sdfs_client : public tcp_client {
public:
    sdfs_client(std::string master_hostname, std::string fs_port) :
        master_hostname(master_hostname), fs_port(fs_port) {}
    /*
     * handles the put operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    int put_operation(std::string local_filename, std::string sdfs_filename);
    /*
     * handles the get operation with the given arguments
     * returns 0 on success, -1 on failure
     **/
    int get_operation(std::string local_filename, std::string sdfs_filename);
    /*
     * handles the delete operation with the given argument
     * returns 0 on success, -1 on failure
     **/
    int delete_operation(std::string sdfs_filename);
    /*
     * handles the ls operation with the given argument
     * returns 0 on success, -1 on failure
     **/
    int ls_operation(std::string sdfs_filename);
    /*
     * handles the store operation
     * returns 0 on success, -1 on failure
     **/
    int store_operation();
private:
    std::string master_hostname;
    std::string fs_port;
};
