#include "sdfs_client.h"

std::string sdfs_client::put_operation(std::string local_filename, std::string sdfs_filename) {
    std::string put_msg = "put " + sdfs_filename;

    // connect and write put request to server
    int socket;
    if ((socket = client.setup_connection(master_hostname, fs_port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, put_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded "OK"
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_ACK_MSG) return SDFS_FAILURE_MSG;

    // if server responded "OK", send the file over the socket
    if (send_file_over_socket(socket, local_filename) == -1) return SDFS_FAILURE_MSG;

    // read the server response and return (hopefully the response is SDFS_SUCCESS_MSG)
    read_ret = client.read_from_server(socket);
    if (read_ret == "") return SDFS_FAILURE_MSG;
    return read_ret;
}

std::string sdfs_client::get_operation(std::string local_filename, std::string sdfs_filename) {
    std::string get_msg = "get " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(master_hostname, fs_port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, get_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded "OK"
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_ACK_MSG) return SDFS_FAILURE_MSG;

    // if server responded "OK", recv the file over the socket
    if (recv_file_over_socket(socket, local_filename) == -1) return SDFS_FAILURE_MSG;

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::delete_operation(std::string sdfs_filename) {
    std::string del_msg = "delete " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(master_hostname, fs_port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, del_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded "OK"
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_ACK_MSG) return SDFS_FAILURE_MSG;

    return read_ret;
}

std::string sdfs_client::ls_operation(std::string sdfs_filename) {
    std::string ls_msg = "ls " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(master_hostname, fs_port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, ls_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded "OK"
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_ACK_MSG) return SDFS_FAILURE_MSG;

    // if server responded "OK", read the ls results over the socket
    if ((read_ret = client.read_from_server(socket)) == "") return SDFS_FAILURE_MSG;
    return read_ret;
}

std::string sdfs_client::store_operation() {
    // this one is a local operation
    return 0;
}

int sdfs_client::send_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
    std::ifstream t(filename);
    std::string file_str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

    return client.write_to_server(socket, file_str);
}

int sdfs_client::recv_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/15388041/how-to-write-stdstring-to-file
    std::ofstream file(filename);
    std::string read_ret = client.read_from_server(socket);
    if (read_ret == "") return -1;

    file << read_ret;
    return 0;
}
