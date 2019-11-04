#include "sdfs_server.h"
void sdfs_server::process_client() {
    // accept the connection on the server
    int client = server.accept_connection();
    if (client == -1) return;
    std::string request = server.read_from_client(client);

    std::istringstream req_stream(request);
    std::vector<std::string> tokens{std::istream_iterator<std::string>{req_stream},
                                    std::istream_iterator<std::string>{}};

    std::cout << "server recvd request " << request << std::endl;
    std::string cmd = tokens[0];
    if (cmd == "put") {
        put_operation(client, tokens[1]);
    } else if (cmd == "get") {
        get_operation(client, tokens[1]);
    } else if (cmd == "delete") {
        delete_operation(client, tokens[1]);
    } else if (cmd == "ls") {
        ls_operation(client, tokens[1]);
    }

    std::cout << "server finished processing request " << request << std::endl;
    server.close_connection(client);
    return;
}

int sdfs_server::put_operation(int client, std::string filename) {
    // send the OK ack to the client
    if (server.write_to_client(client, SDFS_ACK_MSG) == -1) return -1;
    std::cout << "server sent ACK" << std::endl;

    // recv the file from the client
    if (recv_file_over_socket(client, filename) == -1) return -1;
    std::cout << "server recvd file" << std::endl;

    // send the client the success message
    if (server.write_to_client(client, SDFS_SUCCESS_MSG) == -1) return -1;
    std::cout << "server sent success" << std::endl;

    return 0;
}

int sdfs_server::get_operation(int client, std::string filename) {
    // send the OK ack to the client
    if (server.write_to_client(client, SDFS_ACK_MSG) == -1) return -1;

    // send the file to the client
    // #define SDFS_DIR "~/.sdfs"
    if (sdfs_server::send_file_over_socket(client, std::string(SDFS_DIR) + "/" + filename) == -1) return -1;

    return 0;
}

int sdfs_server::delete_operation(int client, std::string filename) {
    // remove the file
    // #define SDFS_DIR "~/.sdfs"
    if (remove((std::string(SDFS_DIR) + "/" + filename).c_str()) == -1) {
        server.write_to_client(client, SDFS_FAILURE_MSG);
        return -1;
    }

    // send success reponse
    if (server.write_to_client(client, SDFS_SUCCESS_MSG) == -1) return -1;

    return 0;
}

int sdfs_server::ls_operation(int client, std::string filename) {
    // check if the specified file exists
    struct stat buffer;
    bool exists = (stat(filename.c_str(), &buffer) == -1);
    if (exists) {
        // send success response if the file exists
        if (server.write_to_client(client, SDFS_SUCCESS_MSG) == -1) return -1;
    } else {
        // send failure response if the file doesn't exist
        if (server.write_to_client(client, SDFS_FAILURE_MSG) == -1) return -1;
    }
    return 0;
}

int sdfs_server::put_operation_mn(int client, std::string filename) {
    // the put operation as master node needs to send the client
    // back the hostname and the port that the filename hashes to

    // the master node should also be responsible for orchestrating the
    // replication of this file, presumably by ordering the receiving node
    // to send it to two other nodes (might need to add a spin of the put command for this)
    // e.g. a prefix command that simply means run this command locallyA
    return 0;
}

int sdfs_server::get_operation_mn(int client, std::string filename) {
    // this get operation as master node should simply send the client
    // the hostname and the port of a node that has the specified file
    // this can easily be acheived by calling ls on the nodes returned
    // by repeated hashing (to simplify hashing, always mod 10 then just
    // check if the result is within the boundaries of mem_list size)
    return 0;
}

int sdfs_server::delete_operation_mn(int client, std::string filename) {
    // this operation should presumably just call delete
    // on every node in the membership list - not scalable ordinarily,
    // but simple and effective for a size 10 max membership list
    return 0;
}

int sdfs_server::ls_operation_mn(int client, std::string filename) {
    // similar to delete, this should simply call ls on all nodes
    // in the membership list - again, not scalable but a decent
    // enough solution for a max size of 10 members
    return 0;
}

int sdfs_server::send_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
    std::ifstream file(string(SDFS_DIR) + filename);
    std::stringstream file_buffer;
    file_buffer << file.rdbuf();

    return server.write_to_client(socket, file_buffer.str());
}

int sdfs_server::recv_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/15388041/how-to-write-stdstring-to-file
    std::string full_path = std::string(SDFS_DIR) + filename;
    std::ofstream file(full_path);
    std::string read_ret = server.read_from_client(socket);
    if (read_ret == "") return -1;

    std::cout << "server received contents " << read_ret << std::endl;
    std::cout << "server is writing to " << full_path << std::endl;
    file << read_ret;
    return 0;
}
