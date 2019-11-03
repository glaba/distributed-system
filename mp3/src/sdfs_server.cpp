#include "sdfs_server.h"

void sdfs_server::process_client(int client) {
    std::string request = server.read_from_client(client);

    std::istringstream req_stream(request);
    std::vector<std::string> tokens{std::istream_iterator<std::string>{req_stream},
                                    std::istream_iterator<std::string>{}};

    std::string cmd = tokens[0];
    if (cmd == "put") {
        put_operation(client, tokens[1]);
    } else if (cmd == "get") {
        get_operation(client, tokens[1]);
    } else if (cmd == "delete") {
        delete_operation(client, tokens[1]);
    }

    return;
}

int sdfs_server::put_operation(int client, std::string filename) {
    // send the OK ack to the client
    if (server.write_to_client(client, SDFS_ACK_MSG) != 0) return -1;

    // recv the file from the client
    if (recv_file_over_socket(client, filename) != 0) return -1;

    // send the client the success message
    if (server.write_to_client(client, SDFS_SUCCESS_MSG) != 0) return -1;

    return 0;
}

int sdfs_server::get_operation(int client, std::string filename) {
    // send the OK ack to the client
    if (server.write_to_client(client, SDFS_ACK_MSG) != 0) return -1;

    // send the file to the client
    // #define SDFS_DIR "~/.sdfs"
    if (sdfs_server::send_file_over_socket(client, std::string(SDFS_DIR) + "/" + filename)!= 0) return -1;

    return 0;
}

int sdfs_server::delete_operation(int client, std::string filename) {
    // remove the file
    // #define SDFS_DIR "~/.sdfs"
    if (remove((std::string(SDFS_DIR) + "/" + filename).c_str()) != 0) {
        server.write_to_client(client, SDFS_FAILURE_MSG);
        return -1;
    }

    // send success reponse
    if (server.write_to_client(client, SDFS_SUCCESS_MSG) != 0) return -1;

    return 0;
}

int sdfs_server::send_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
    std::ifstream file(filename);
    std::string file_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return server.write_to_client(socket, file_str);
}

int sdfs_server::recv_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/15388041/how-to-write-stdstring-to-file
    std::ofstream file(filename);
    std::string read_ret = server.read_from_client(socket);
    if (read_ret == "") return -1;

    file << read_ret;
    return 0;
}
