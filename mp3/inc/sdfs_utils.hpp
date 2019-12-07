#include "tcp.h"
#include "logging.h"
#include "sdfs_message.h"

#include <cstring>

// Defining the return value for failed operations
#define SDFS_FAILURE -1
// Defining the return value for successful operations
#define SDFS_SUCCESS 0

class sdfs_utils {
public:
    static int send_message(tcp_client *client, int socket, sdfs_message sdfs_msg);
    static int receive_message(tcp_client *client, int socket, sdfs_message *sdfs_msg);
    static int send_message(tcp_server *server, int socket, sdfs_message sdfs_msg);
    static int receive_message(tcp_server *server, int socket, sdfs_message *sdfs_msg);
    static ssize_t write_file_to_socket(tcp_client *client, int socket, std::string filename);
    static ssize_t read_file_from_socket(tcp_client *client, int socket, std::string filename);
    static ssize_t write_file_to_socket(tcp_server *server, int socket, std::string filename);
    static ssize_t read_file_from_socket(tcp_server *server, int socket, std::string filename);
private:
    int acquire_lock(int fd, int operation);
    int release_lock(int fd);
};
