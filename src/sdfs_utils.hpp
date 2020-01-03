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
    static auto send_message(tcp_client *client, sdfs_message const& sdfs_msg) -> int;
    static auto receive_message(tcp_client *client, sdfs_message *sdfs_msg) -> int;
    static auto send_message(tcp_server *server, int socket, sdfs_message const& sdfs_msg) -> int;
    static auto receive_message(tcp_server *server, int socket, sdfs_message *sdfs_msg) -> int;

    static auto write_file_to_socket(tcp_client *client, std::string const& filename) -> ssize_t;
    static auto read_file_from_socket(tcp_client *client, std::string const& filename) -> ssize_t;
    static auto write_file_to_socket(tcp_server *server, int socket, std::string const& filename) -> ssize_t;
    static auto read_file_from_socket(tcp_server *server, int socket, std::string const& filename) -> ssize_t;

    static auto write_first_line_to_socket(tcp_server *server, int socket, std::string const& filename) -> ssize_t;
private:
    static auto acquire_lock(int fd, int operation) -> int;
    static auto release_lock(int fd) -> int;
};
