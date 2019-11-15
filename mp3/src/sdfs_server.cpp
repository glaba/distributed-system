#include "sdfs_server.h"

#include <chrono>
#include <string>

void sdfs_server::start() {
    std::thread *server_thread = new std::thread([this] {process_loop();});
    server_thread->detach();
}

void sdfs_server::process_loop() {
    while (true) {
        int client = server.accept_connection();
        if (client == -1) continue;
        // process_client(client);
        std::thread *client_t = new std::thread([this, client] {process_client(client);});
        client_t->detach();
        // accept the connection on the server
    }
}

void sdfs_server::process_client(int client) {
    std::string request = server.read_from_client(client);

    std::istringstream req_stream(request);
    std::vector<std::string> tokens{std::istream_iterator<std::string>{req_stream},
                                    std::istream_iterator<std::string>{}};

    bool succeeded;
    member master = el->get_master_node(&succeeded);

    if (tokens[0] == "mn" && succeeded && master.hostname == hostname) {
        tokens.erase(tokens.begin());
        std::string cmd = tokens[0];
        if (cmd == "put") {
            if (tokens[2] == "force") {
                put_operation_mn(client, tokens[1], true);
            } else if (tokens[2] == "noforce") {
                put_operation_mn(client, tokens[1], false);
            }
        } else if (cmd == "get") {
            get_operation_mn(client, tokens[1]);
        } else if (cmd == "delete") {
            delete_operation_mn(client, tokens[1]);
        } else if (cmd == "ls") {
            ls_operation_mn(client, tokens[1]);
        }
    } else if (tokens[0] == "r") {
        // relay request
        relay_operation(tokens[1]);
        /*
        tokens.erase(tokens.begin());
        std::string hostname = tokens[0];
        tokens.erase(tokens.begin());
        std::string cmd = tokens[0];
        if (cmd == "put") {
            std::string full_path = std::string(SDFS_DIR) + tokens[1];
            sdfsc->put_operation(hostname, full_path, tokens[1]);
        } else if (cmd == "get") {
            // get_operation_mn(client, tokens[1]);
        } else if (cmd == "delete") {
            // delete_operation_mn(client, tokens[1]);
        } else if (cmd == "ls") {
            // ls_operation_mn(client, tokens[1]);
        }
        */
    } else {
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
    }

    // std::cout << "finished processing request " << request << std::endl;
    server.close_connection(client);
    return;
}

int sdfs_server::put_operation(int client, std::string filename) {
    // send the OK ack to the client
    if (server.write_to_client(client, SDFS_ACK_MSG) == -1) return -1;

    // recv the file from the client
    // if (recv_file_over_socket(client, filename) == -1) return -1;

    // send the client the success message
    if (server.write_to_client(client, SDFS_SUCCESS_MSG) == -1) return -1;

    lg->info("successful completion of put request for file " + filename);
    return 0;
}

int sdfs_server::get_operation(int client, std::string filename) {
    // send the OK ack to the client
    if (server.write_to_client(client, SDFS_ACK_MSG) == -1) return -1;

    // send the file to the client
    // #define SDFS_DIR "~/.sdfs"
    // if (sdfs_server::send_file_over_socket(client, filename) == -1) return -1;

    lg->info("successful completion of get request for file " + filename);
    return 0;
}

int sdfs_server::delete_operation(int client, std::string filename) {
    // remove the file
    // #define SDFS_DIR "~/.sdfs"
    std::string full_path = std::string(SDFS_DIR) + filename;
    if (remove(full_path.c_str()) == -1) {
        server.write_to_client(client, SDFS_FAILURE_MSG);
        return -1;
    }

    // send success reponse
    if (server.write_to_client(client, SDFS_SUCCESS_MSG) == -1) return -1;

    lg->info("successful completion of delete request for file " + filename);
    return 0;
}

int sdfs_server::ls_operation(int client, std::string filename) {
    // check if the specified file exists
    struct stat buffer;
    std::string full_path = std::string(SDFS_DIR) + filename;
    bool exists = (stat(full_path.c_str(), &buffer) == 0);
    if (exists) {
        // send success response if the file exists
        if (server.write_to_client(client, SDFS_SUCCESS_MSG) == -1) return -1;
    } else {
        // send failure response if the file doesn't exist
        if (server.write_to_client(client, SDFS_FAILURE_MSG) == -1) return -1;
    }

    lg->info("successful completion of ls request for file " + filename);
    return 0;
}

int sdfs_server::put_operation_mn(int client, std::string filename, bool force) {
    // the put operation as master node needs to send the client
    // back the hostname and the port that the filename hashes to

    // the master node should also be responsible for orchestrating the
    // replication of this file, presumably by ordering the receiving node
    // to send it to two other nodes (might need to add a spin of the put command for this)
    // e.g. a prefix command that simply means run this command locally

    // get list of destinations
    std::vector<member> destinations = get_file_destinations(filename);

    if (!force) {
        // Check the 1 minute timeout
        uint64_t now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        if (now - update_times[filename] < 60000) {
            server.write_to_client(client, "!");
            now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

            std::string response = server.read_from_client(client);
            if (response == "Y") {
                uint64_t new_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                // Check that 30 seconds haven't passed
                if (new_time - now >= 30000) {
                    server.write_to_client(client, "F");
                }
            } else {
                return 0;
            }
        }
    }

    // write the initial host to client
    server.write_to_client(client, "Y" + destinations[0].hostname);
    update_times[filename] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    // receive the response from the client (how did the file transfer go?)
    std::string response = server.read_from_client(client);
    if (response != SDFS_SUCCESS_MSG) return -1;

    sdfsc->relay_operation(destinations[0].hostname, filename);
    /*
    for (unsigned i = 1; i < destinations.size(); i++) {
        // relay put for each destination
        std::string local = std::string(SDFS_DIR) + filename;
        std::string operation = std::string("put ") + local + " " + filename;
        sdfsc->relay_operation(destinations[0].hostname, destinations[i].hostname, operation);
    }
    */

    return 0;
}

int sdfs_server::get_operation_mn(int client, std::string filename) {
    // this get operation as master node should simply send the client
    // the hostname and the port of a node that has the specified file
    // this can easily be acheived by calling ls on the nodes returned
    // by repeated hashing (to simplify hashing, always mod 10 then just
    // check if the result is within the boundaries of mem_list size)

    // @TODO: move hashing and ls logic to the server
    std::vector<member> mems = hb->get_members();
    send_client_mem_vector(client, mems);

    // get_operation(client, filename);
    return 0;
}

int sdfs_server::delete_operation_mn(int client, std::string filename) {
    // this operation should presumably just call delete
    // on every node in the membership list - not scalable ordinarily,
    // but simple and effective for a size 10 max membership list

    // @TODO: write a clone request for the mn that will
    // send requests to be carried out by the recv node
    // for now : simple solution - the same as ls.

    std::vector<member> mems = hb->get_members();
    send_client_mem_vector(client, mems);

    // this should be all for now - should be revised later
    return 0;
}

int sdfs_server::ls_operation_mn(int client, std::string filename) {
    // similar to delete, this should simply call ls on all nodes
    // in the membership list - again, not scalable but a decent
    // enough solution for a max size of 10 members

    std::vector<member> mems = hb->get_members();

    send_client_mem_vector(client, mems);

    // this should be all for now - should be revised later
    return 0;
}

int sdfs_server::relay_operation(std::string filename) {
    std::string local_filename = std::string(SDFS_DIR) + filename;
    std::vector<member> destinations = get_file_destinations(filename);
    for (auto dest : destinations) {
        std::cout << "destination " << dest.hostname << std::endl;
        sdfsc->put_operation(dest.hostname, local_filename, filename);
    }
}

void fix_replicas(member failed_member) {
    // should only be done if the current node is the master node
    // @TODO: write this code - loop through list of files stored by failed node
    // and add them appropriately
}

std::vector<member> sdfs_server::get_file_destinations(std::string filename) {
    std::vector<member> results;

    std::vector<member> members = hb->get_members();
    int num_members = members.size();

    // replica count is 4, unless there are fewer than 4 nodes in the network
    int num_required_replicas = num_members < 4 ? num_members : 3;

    // a bit cheeky but i'm going to hash with the mod value of 10 and just check the result
    // this is so that files don't need to be moved around on node failure to match new hashing

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

void sdfs_server::send_client_mem_vector(int client, std::vector<member> vec) {
    // first write to the client the number of members that will be sent
    server.write_to_client(client, std::to_string(vec.size()).c_str());

    // now send over the information of each member
    // for the client to send an ls request to
    for (auto mem : vec) {
        server.write_to_client(client, mem.hostname.c_str());
    }
}

int sdfs_server::send_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
    std::string full_path = std::string(SDFS_DIR) + filename;
    std::ifstream file(full_path);
    std::stringstream file_buffer;
    file_buffer << file.rdbuf();

    return server.write_to_client(socket, file_buffer.str());
}

int sdfs_server::recv_file_over_socket(int socket, std::string filename) {
    // https://stackoverflow.com/questions/15388041/how-to-write-stdstring-to-file
    std::string full_path = std::string(SDFS_DIR) + filename;
    std::ofstream file(full_path);
    std::string read_ret = server.read_from_client(socket);

    file << read_ret;
    return 0;
}
