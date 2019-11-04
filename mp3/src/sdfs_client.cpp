#include "sdfs_client.h"

#include <string>

void sdfs_client::start() {
    input_loop();
}

void sdfs_client::input_loop() {
    std::string request;
    while (true) {
        std::cout << "enter an sdfs command (e.g. \"help\"): ";
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

        if (tokens.empty()) {
            std::cout << "invalid command" << std::endl;
            continue;
        }

        if (tokens[0] == "help") {
            std::cout << "AVAILABLE COMMANDS : \n\t put <local_filename> <sdfs_filename> \
                          \n\t get <local_filename> <sdfs_filename> \
                          \n\t delete <sdfs_filename> \
                          \n\t ls <sdfs_filename> \
                          \n\t store \
                          \n\t mem_list \
                          \n\t our_id \
                          \n\t leave_group \
                          \n\t exit" << std::endl;
        } else if (tokens[0] == "put") {
            if (tokens.size() == 3) {
                std::string ret = put_operation_wr(m_hostname, tokens[1], tokens[2]);
                if (ret == SDFS_SUCCESS_MSG) std::cout << "put was a success" << std::endl;
                else std::cout << "put failed" << std::endl;
            } else {
                std::cout << "invalid options for put request" << std::endl;
            }
        } else if (tokens[0] == "get") {
            if (tokens.size() == 3) {
                std::string ret = get_operation_wr(m_hostname, tokens[1], tokens[2]);
                if (ret == SDFS_SUCCESS_MSG) std::cout << "get was a success" << std::endl;
                else std::cout << "get failed" << std::endl;
            } else {
                std::cout << "invalid options for get request" << std::endl;
            }
        } else if (tokens[0] == "delete") {
            if (tokens.size() == 2) {
                std::string ret = delete_operation_wr(m_hostname, tokens[1]);
                if (ret == SDFS_SUCCESS_MSG) std::cout << "delete was a success" << std::endl;
                else std::cout << "delete failed" << std::endl;
            } else {
                std::cout << "invalid options for delete request" << std::endl;
            }
        } else if (tokens[0] == "ls") {
            if (tokens.size() == 2) {
                std::string ret = ls_operation_wr(m_hostname, tokens[1]);
                if (ret == SDFS_FAILURE_MSG) std::cout << "ls failed" << std::endl;
            } else {
                std::cout << "invalid options for ls request" << std::endl;
            }
        } else if (tokens[0] == "store") {
            store_operation();
        } else if (tokens[0] == "mem_list") {
            std::vector<member> members = hb->get_members();
            std::cout << "Hostname\t\tID" << std::endl;
            for (auto m : members) {
                std::cout << m.hostname << "\t\t" << m.id << std::endl;
            }
        } else if (tokens[0] == "our_id") {
            std::cout << hb->get_id();
        } else if (tokens[0] == "leave_group") {
            hb->leave_group();
        } else if (tokens[0] == "exit") {
            return;
        } else {
            std::cout << "invalid command" << std::endl;
        }
    }
    return;
}

std::string sdfs_client::put_operation_wr(std::string hostname, std::string local_filename, std::string sdfs_filename) {
    // hostname refers to master hostname

    // get the list of destinations from the master node
    // and call the put operation on all the destinations
    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) return SDFS_FAILURE_MSG;
    std::string put_mn_msg = "mn put " + sdfs_filename + " noforce";
    if (client.write_to_server(socket, put_mn_msg) == -1) return SDFS_FAILURE_MSG;

    // get destination host for file sending
    std::string response = client.read_from_server(socket);
    if (response == "!") {
        std::string request;
        std::cout << "Are you sure (Y/N): " << std::flush;
        std::getline(std::cin, request);
        if (request == "Y") {
            client.write_to_server(socket, "Y");
        } else {
            client.write_to_server(socket, "N");
            return SDFS_FAILURE_MSG;
        }
        response = client.read_from_server(socket);
        if (response == "F") {
            std::cout << "Confirmation timed out" << std::endl;
            return SDFS_FAILURE_MSG;
        }
    }

    std::string destination = response.substr(1, std::string::npos);

    // put the file
    put_operation(destination, local_filename, sdfs_filename);

    // do the puts to the other nodes as well

    // send success message after the put occurs
    client.write_to_server(socket, SDFS_SUCCESS_MSG);

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::get_operation_wr(std::string hostname, std::string local_filename, std::string sdfs_filename) {
    // hostname refers to master hostname

    // get the list of member from the master node
    // and hash and ls until the file is found

    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) return SDFS_FAILURE_MSG;
    std::string put_mn_msg = "mn get " + sdfs_filename;
    if (client.write_to_server(socket, put_mn_msg) == -1) return SDFS_FAILURE_MSG;

    // get the mems and close the mn socket
    std::vector<std::string> members = recv_mems_over_socket(socket);
    client.close_connection(socket);

    std::string ret = get_file_location(members, sdfs_filename);
    if (ret == SDFS_FAILURE_MSG) return SDFS_FAILURE_MSG;

    get_operation(ret, local_filename, sdfs_filename);

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::delete_operation_wr(std::string hostname, std::string sdfs_filename) {
    // hostname refers to master hostname
    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) return SDFS_FAILURE_MSG;
    std::string put_mn_msg = "mn delete " + sdfs_filename;
    if (client.write_to_server(socket, put_mn_msg) == -1) return SDFS_FAILURE_MSG;

    // get the mems and close the mn socket
    std::vector<std::string> members = recv_mems_over_socket(socket);
    client.close_connection(socket);

    for (auto mem : members) {
        delete_operation(mem, sdfs_filename);
    }

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::ls_operation_wr(std::string hostname, std::string sdfs_filename) {
    // hostname refers to master hostname
    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) return SDFS_FAILURE_MSG;
    std::string put_mn_msg = "mn delete " + sdfs_filename;
    if (client.write_to_server(socket, put_mn_msg) == -1) return SDFS_FAILURE_MSG;

    // get the mems and close the mn socket
    std::vector<std::string> members = recv_mems_over_socket(socket);
    client.close_connection(socket);

    std::vector<std::string> results;
    for (auto mem : members) {
        if (ls_operation(mem, sdfs_filename) == SDFS_SUCCESS_MSG) results.push_back(mem);
    }

    if (results.size() == 0) std::cout << sdfs_filename << " was not found on any machines" << std::endl;
    else std::cout << sdfs_filename << " was found on the following machines:" << std::endl;
    for (auto res : results) {std::cout << res << std::endl;}

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::put_operation(std::string hostname, std::string local_filename, std::string sdfs_filename) {
    int start_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::string put_msg = "put " + sdfs_filename;

    // connect and write put request to server
    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) {return SDFS_FAILURE_MSG;}
    if (client.write_to_server(socket, put_msg) == -1) {client.close_connection(socket); return SDFS_FAILURE_MSG;}

    // read server response - proceed if server responded "OK"
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_ACK_MSG) {client.close_connection(socket); return SDFS_FAILURE_MSG;}

    // if server responded "OK", send the file over the socket
    int ret;
    /*
    if ((ret = send_file_over_socket(socket, local_filename)) <= 0) {
        if (ret <= 0) system((std::string("scp " + local_filename + " lawsonp2@" + hostname + ":~/.sdfs/" + sdfs_filename).c_str()));
        client.close_connection(socket); return SDFS_FAILURE_MSG;
    }
    */
    ret = system((std::string("scp -q " + local_filename + " lawsonp2@" + hostname + ":~/.sdfs/" + sdfs_filename).c_str()));

    // read the server response and return (hopefully the response is SDFS_SUCCESS_MSG)
    read_ret = client.read_from_server(socket);
    if (read_ret == "") {client.close_connection(socket); return SDFS_FAILURE_MSG;}

    client.close_connection(socket);
    int end_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    int total_time = (end_time - start_time);
    // std::cout << "total time taken was " <<  total_time << " ms" << std::endl;
    return read_ret;
}

std::string sdfs_client::get_operation(std::string hostname, std::string local_filename, std::string sdfs_filename) {
    int start_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::string get_msg = "get " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, get_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded "OK"
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_ACK_MSG) return SDFS_FAILURE_MSG;


    // if server responded "OK", recv the file over the socket
    // if (recv_file_over_socket(socket, local_filename) == -1) return SDFS_FAILURE_MSG;
    int ret = system((std::string("scp -q ") + "lawsonp2@" + hostname + ":~/.sdfs/" + sdfs_filename + " " + local_filename).c_str());

    int end_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    int total_time = (end_time - start_time);
    // std::cout << "total time taken was " <<  total_time << " ms" << std::endl;
    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::delete_operation(std::string hostname, std::string sdfs_filename) {
    std::string del_msg = "delete " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, del_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded SDFS_SUCCESS_MSG
    std::string read_ret = client.read_from_server(socket);
    if (read_ret != SDFS_SUCCESS_MSG) return SDFS_FAILURE_MSG;

    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::ls_operation(std::string hostname, std::string sdfs_filename) {
    std::string ls_msg = "ls " + sdfs_filename;

    // connect and write get request to server
    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, ls_msg) == -1) return SDFS_FAILURE_MSG;

    // read server response - proceed if server responded "OK"
    std::string read_ret;
    if ((read_ret = client.read_from_server(socket)) == "") return SDFS_FAILURE_MSG;
    return read_ret;
}

std::string sdfs_client::store_operation() {
    // this one is a local operation
    DIR *dirp = opendir(std::string(SDFS_DIR).c_str());
    struct dirent *dp;

    while ((dp = readdir(dirp)) != NULL) {
        std::cout << dp->d_name << std::endl;;
    }

    closedir(dirp);
    return "";
}

std::string sdfs_client::relay_operation(std::string hostname, std::string relay_hostname, std::string operation) {
    std::string relay_msg = "relay " + relay_hostname + " " + operation;

    // connect and write relay request to server
    int socket;
    if ((socket = client.setup_connection(hostname, protocol_port)) == -1) return SDFS_FAILURE_MSG;
    if (client.write_to_server(socket, relay_msg) == -1) return SDFS_FAILURE_MSG;
    return SDFS_SUCCESS_MSG;
}

std::string sdfs_client::get_file_location(std::vector<std::string> members, std::string filename) {
    int num_members = members.size();

    std::hash<std::string> hasher;
    int i = 0;
    int curr_hash = hasher(filename) % 10;
    while (i < 10) {
        if (curr_hash < num_members && ls_operation(members[curr_hash], filename) == SDFS_SUCCESS_MSG) return members[curr_hash];
        curr_hash = (curr_hash + 1) % 10; i++;
    }

    return SDFS_FAILURE_MSG;
}

std::vector<std::string> sdfs_client::recv_mems_over_socket(int socket) {
    std::string::size_type sz;
    int num_mems = std::stoi(client.read_from_server(socket), &sz);
    std::vector<std::string> hosts;

    for (int i = 0; i < num_mems; i++) {
        std::string hostname = client.read_from_server(socket);
        hosts.push_back(hostname);
    }

    return hosts;
}

std::vector<member> sdfs_client::get_file_destinations(std::string filename) {
    std::vector<member> results;

    std::vector<member> members = hb->get_members();
    int num_members = members.size();

    // replica count is 4, unless there are fewer than 4 nodes in the network
    int num_required_replicas = num_members < 4 ? num_members : 3;

    // a bit cheeky but i'm going to hash with the mod value of 10 and just check the result
    // this is so that files don't need to be moved around on node failure to match new hashing

    // std::cout << "number of members : " << num_members << std::endl;
    // std::cout << "number of required replicas : " << num_required_replicas << std::endl;

    std::hash<std::string> hasher;
    int curr_hash = hasher(filename) % 10;
    for (int i = 0; i < num_required_replicas; i++) {
        while (true) {
            if (curr_hash < num_members && std::find(results.begin(), results.end(), members[curr_hash]) == results.end()) break;
            curr_hash = (curr_hash + 1) % 10;
        }
        results.push_back(members[curr_hash]);
    }

    return results;
}

int sdfs_client::send_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
    std::ifstream file(filename);
    std::stringstream file_buffer;
    file_buffer << file.rdbuf();

    std::string buf = file_buffer.str();
    int ret = client.write_to_server(socket, buf);
    return ret;
}

int sdfs_client::recv_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/15388041/how-to-write-stdstring-to-file
    std::ofstream file(filename);
    std::string read_ret = client.read_from_server(socket);

    file << read_ret;
    return 0;
}
