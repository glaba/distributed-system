#include "sdfs_client.h"

void sdfs_client::input_loop() {
    std::string request;
    while (true) {
        std::getline(std::cin, request);
        std::istringstream req_stream(request);
        std::vector<std::string> tokens{std::istream_iterator<std::string>{req_stream},
                                        std::istream_iterator<std::string>{}};


        bool succeeded;
        member master = el->get_master_node(&succeeded);
        while (!succeeded) {
            master = el->get_master_node(&succeeded);
        }

        std::string m_hostname = master.hostname;

        if (tokens[0] == "help") {
            std::cout << "AVAILABLE COMMANDS : \n\t put <local_filename> <sdfs_filename> \
                          \n\t get <local_filename> <sdfs_filename> \
                          \n\t delete <sdfs_filename> \
                          \n\t ls <sdfs_filename> \
                          \n\t store" << std::endl;
        } else if (tokens[0] == "put") {
            if (tokens.size() == 3) {
                put_operation_wr(m_hostname, protocol_port, tokens[1], tokens[2]);
            } else {
                std::cout << "invalid options for put request" << std::endl;
            }
        } else if (tokens[0] == "get") {
            if (tokens.size() == 3) {
                get_operation(m_hostname, protocol_port, tokens[1], tokens[2]);
            } else {
                std::cout << "invalid options for get request" << std::endl;
            }
        } else if (tokens[0] == "delete") {
            if (tokens.size() == 2) {
                delete_operation(m_hostname, protocol_port, tokens[1]);
            } else {
                std::cout << "invalid options for delete request" << std::endl;
            }
        } else if (tokens[0] == "ls") {
            if (tokens.size() == 2) {
                ls_operation(m_hostname, protocol_port, tokens[1]);
            } else {
                std::cout << "invalid options for ls request" << std::endl;
            }
        } else if (tokens[0] == "store") {
            store_operation();
        }
    }
    return;
}

std::string sdfs_client::put_operation_wr(std::string hostname, std::string port, std::string local_filename, std::string sdfs_filename) {
    // hostname refers to master hostname

    // get the list of destinations from the master node
    // and call the put operation on all the destinations
    int socket;
    if ((socket = client.setup_connection(hostname, port)) == -1) return SDFS_FAILURE_MSG;

    std::vector<std::string> destinations = recv_mems_over_socket(socket);
    for (auto dest : destinations) {
        std::cout << dest << std::endl;
        // put_operation(dest, protocol_port, local_filename, sdfs_filename);
    }

    return SDFS_SUCCESS_MSG;
}

std::string get_operation_wr(std::string hostname, std::string port, std::string local_filename, std::string sdfs_filename) {
    // hostname refers to master hostname

    // get the list of member from the master node
    // and hash and ls until the file is found
    return SDFS_SUCCESS_MSG;
}

std::string delete_operation_wr(std::string hostname, std::string port, std::string sdfs_filename) {
    // hostname refers to master hostname

    return SDFS_SUCCESS_MSG;
}

std::string ls_operation_wr(std::string hostname, std::string port, std::string sdfs_filename) {
    // hostname refers to master hostname

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::put_operation(std::string hostname, std::string port, std::string local_filename, std::string sdfs_filename) {
    std::string put_msg = "put " + sdfs_filename;

    // connect and write put request to server
    int socket;
    if ((socket = client.setup_connection(hostname, port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, put_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded "OK"
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_ACK_MSG) return SDFS_FAILURE_MSG;

    // if server responded "OK", send the file over the socket
    if (send_file_over_socket(socket, local_filename) == -1) return SDFS_FAILURE_MSG;

    // read the server response and return (hopefully the response is SDFS_SUCCESS_MSG)
    read_ret = client.read_from_server(socket);
    if (read_ret == "") return SDFS_FAILURE_MSG;

    client.close_connection(socket);
    return read_ret;
}

std::string sdfs_client::get_operation(std::string hostname, std::string port, std::string local_filename, std::string sdfs_filename) {
    std::string get_msg = "get " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(hostname, port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, get_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded "OK"
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_ACK_MSG) return SDFS_FAILURE_MSG;

    // if server responded "OK", recv the file over the socket
    if (recv_file_over_socket(socket, local_filename) == -1) return SDFS_FAILURE_MSG;

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::delete_operation(std::string hostname, std::string port, std::string sdfs_filename) {
    std::string del_msg = "delete " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(hostname, port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, del_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded SDFS_SUCCESS_MSG
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_SUCCESS_MSG) return SDFS_FAILURE_MSG;

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::ls_operation(std::string hostname, std::string port, std::string sdfs_filename) {
    std::string ls_msg = "ls " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(hostname, port)) == -1) return SDFS_FAILURE_MSG;
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
    DIR *dirp = opendir(std::string(SDFS_DIR).c_str());
    struct dirent *dp;

    std::cout << "=== BEGIN SDFS LISTING ===" << std::endl;
    while ((dp = readdir(dirp)) != NULL) {
        std::cout << dp->d_name << std::endl;;
    }
    std::cout << "=== END SDFS LISTING ===" << std::endl;

    closedir(dirp);
    return "";
}

std::vector<std::string> sdfs_client::recv_mems_over_socket(int socket) {
    std::string::size_type sz;
    int num_mems = std::stoi(client.read_from_server(socket), &sz);
    std::cout << "CLIENT RECEIVED " << num_mems << "MEMS" << std::endl;
    std::vector<std::string> hosts;

    for (int i = 0; i < num_mems; i++) {
        std::string hostname = client.read_from_server(socket);
        hosts.push_back(hostname);
    }

    return hosts;
}

int sdfs_client::send_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
    std::ifstream file(filename);
    std::stringstream file_buffer;
    file_buffer << file.rdbuf();

    return client.write_to_server(socket, file_buffer.str());
}

int sdfs_client::recv_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/15388041/how-to-write-stdstring-to-file
    std::ofstream file(filename);
    std::string read_ret = client.read_from_server(socket);

    file << read_ret;
    return 0;
}
