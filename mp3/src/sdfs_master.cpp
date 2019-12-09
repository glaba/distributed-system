#include "sdfs_master.hpp"

sdfs_master_impl::sdfs_master_impl(environment &env)
    : el(env.get<election>()),
      hb(env.get<heartbeater>()),
      lg(env.get<logger_factory>()->get_logger("sdfs_master")),
      client(env.get<tcp_factory>()->get_tcp_client()),
      server(env.get<tcp_factory>()->get_tcp_server()),
      config(env.get<configuration>())
{

}

void sdfs_master_impl::start() {
    // @TODO: ADD LOGIC TO ACCEPT CLIENT CONNECTIONS AND SPIN UP NEW THREADS
    // @TODO: THIS SHOULD BE A CALLBACK FROM A NEW ELECTION AND SHOULD CHECK IF ELECTED

    // @TODO: THIS FUNCTION SHOULD ACCEPT NEW CONNECTIONS AND KICK OFF THREADS THAT
    //        HANDLE ERROR CHECKING AND STUFF
    server->setup_server(config->get_sdfs_master_port());
    std::thread *server_thread = new std::thread([this] {process_loop();});
    server_thread->detach();
    return;
}

void sdfs_master_impl::stop() {
    // @TODO: ADD LOGIC TO STOP ACCEPTING CONNECTIONS
    // @TODO: THIS SHOULD PROBABLY BE AS A CALLBACK FROM ELECTION

    server->stop_server();
    return;
}

void sdfs_master_impl::process_loop() {
    while (true) {
        // accept the connection on the server
        int client = server->accept_connection();

        if (client == -1) continue;

        std::thread *client_t = new std::thread([this, client] {handle_connection(client);});
        client_t->detach();
    }
}

void sdfs_master_impl::handle_connection(int socket) {
    // THIS FUNCTION SHOULD HANDLE CONTROL FLOW OF CONNECTION
    // AND CLOSE THE CONNECTION AT THE END -> THIS IS A THREAD ENTRYPOINT

    // receive, parse, and handle request
    // @TODO: what should i do which the operation return values?
    sdfs_message request;
    if (sdfs_utils::receive_message(server.get(), socket, &request) == SDFS_SUCCESS) {
        std::string sdfs_filename = request.get_sdfs_filename();
        if (request.get_type() == sdfs_message::msg_type::put) {
            put_operation(socket, sdfs_filename);
        } else if (request.get_type() == sdfs_message::msg_type::get) {
            get_operation(socket, sdfs_filename);
        } else if (request.get_type() == sdfs_message::msg_type::gmd) {
            get_operation(socket, sdfs_filename);
        } else if (request.get_type() == sdfs_message::msg_type::del) {
            del_operation(socket, sdfs_filename);
        } else if (request.get_type() == sdfs_message::msg_type::ls) {
            ls_operation(socket, sdfs_filename);
        }
    }

    // cleanup
    server->close_connection(socket);
}

void sdfs_master_impl::handle_failures(member failed_node) {
    // first, iterate through list of files this host has
    //        and for each file remove this host from that file's list
    //        and find a suitable host to replicate that file at (call rep op using known host of file)
    // second, clean up the hosts dictionary by removing this hostname
}

int sdfs_master_impl::put_operation(int socket, std::string sdfs_filename) {
    // @TODO: to make this a quorum, may run the replication portion in a diff thread
    //        and either just use 0 index or actually keep track of the original write
    // RECEIVED PUT REQUEST FROM CLIENT
    lg->info("master received client put request for " + sdfs_filename);

    // GET A LIST OF HOSTNAMES TO PUT TO
    std::vector<std::string> hostnames = get_hostnames();
    if (hostnames.size() == 0) return SDFS_FAILURE;

    // RESPOND WITH APPROPRIATE LOCATION FOR CLIENT TO FORWARD REQUEST TO
    sdfs_message sdfs_msg;
    sdfs_msg.set_type_mn_put(hostnames[0]);
    if (sdfs_utils::send_message(server.get(), socket, sdfs_msg) == SDFS_FAILURE) return SDFS_FAILURE;

    // RECEIVE CLIENT COMMAND STATUS MESSAGE
    sdfs_message response;
    if (sdfs_utils::receive_message(server.get(), socket, &response) == SDFS_FAILURE) return SDFS_FAILURE;

    // UPDATE THE MAPS
    file_to_hostnames[sdfs_filename].push_back(hostnames[0]);
    hostname_to_files[hostnames[0]].push_back(sdfs_filename);

    if (response.get_type() == sdfs_message::msg_type::fail) {
        return SDFS_FAILURE;
    } else if (response.get_type() == sdfs_message::msg_type::success) {
        int in_socket;
        if ((in_socket = client->setup_connection(hostnames[0], config->get_sdfs_internal_port())) == -1) return SDFS_FAILURE;
        for (unsigned i = 1; i < hostnames.size(); i++) {
            // @TODO: INSERT ERROR CHECKING AND FURTHER REPLICATION HERE
            // @TODO: UPDATE MAPS ON SUCCESSFUL REPLICATION
            rep_operation(in_socket, hostnames[i], sdfs_filename);
        }
        client->close_connection(in_socket);
        return SDFS_SUCCESS;
    } else {
        return SDFS_FAILURE;
    }

    return SDFS_SUCCESS;
}

int sdfs_master_impl::get_operation(int socket, std::string sdfs_filename) {
    // RECEIVED GET REQUEST FROM CLIENT
    lg->info("master received client get request for " + sdfs_filename);

    // GET PROPER HOSTNAME
    std::string hostname = "";
    if (!sdfs_file_exists(sdfs_filename)) {
        // file does not exist - send msg with "" hostname and return
        sdfs_message empty_host;
        empty_host.set_type_mn_get("");
        if (sdfs_utils::send_message(server.get(), socket, empty_host) == SDFS_FAILURE) return SDFS_FAILURE;
        return SDFS_SUCCESS;
    } else {
        // file exists
        hostname = file_to_hostnames[sdfs_filename][0];
    }

    // RESPOND WITH APPROPRIATE LOCATION FOR CLIENT TO FORWARD REQUEST TO
    sdfs_message sdfs_msg;
    sdfs_msg.set_type_mn_get(hostname);

    if (sdfs_utils::send_message(server.get(), socket, sdfs_msg) == SDFS_FAILURE) return SDFS_FAILURE;

    // RECEIVE CLIENT COMMAND STATUS MESSAGE
    sdfs_message response;
    if (sdfs_utils::receive_message(server.get(), socket, &response) == SDFS_FAILURE) return SDFS_FAILURE;
    if (response.get_type() == sdfs_message::msg_type::success) {
        return SDFS_SUCCESS;
    } else if (response.get_type() == sdfs_message::msg_type::fail) {
        // @TODO: SHOULD I TRY AND REDIRECT THE CLIENT OR JUST LET THEM ERR
        return SDFS_FAILURE;
    } else {
        return SDFS_FAILURE;
    }

    return SDFS_SUCCESS;
}

int sdfs_master_impl::del_operation(int socket, std::string sdfs_filename) {
    // RECEIVED DEL REQUEST FROM CLIENT
    lg->info("master received client del request for " + sdfs_filename);

    if (!sdfs_file_exists(sdfs_filename)) return SDFS_FAILURE;

    int internal_node;

    sdfs_message sdfs_msg;
    sdfs_msg.set_type_del(sdfs_filename);
    std::vector<std::string> hosts = file_to_hostnames[sdfs_filename];
    for (auto host : hosts) {
        if ((internal_node = client->setup_connection(host, config->get_sdfs_internal_port())) != -1) {
            sdfs_utils::send_message(client.get(), internal_node, sdfs_msg);
            hostname_to_files[host].erase(
                    std::remove(hostname_to_files[host].begin(), hostname_to_files[host].end(), sdfs_filename),
                    hostname_to_files[host].end());
            client->close_connection(internal_node);
        }
        file_to_hostnames.erase(sdfs_filename);
    }

    // REPLY TO CLIENT WITH SUCCESS
    sdfs_message response;
    response.set_type_success();
    if (sdfs_utils::send_message(server.get(), socket, response) == -1) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

int sdfs_master_impl::ls_operation(int socket, std::string sdfs_filename) {
    // RECEIVED LS REQUEST FROM CLIENT
    lg->info("master received client ls request for " + sdfs_filename);

    std::string data = "";
    // GET LIST OF NODES WITH THE FILE
    if (sdfs_file_exists(sdfs_filename)) {
        std::vector<std::string> hosts = file_to_hostnames.find(sdfs_filename)->second;
        for (size_t i = 0; i < hosts.size(); i++) {
            if (i == hosts.size() - 1) {
                data += hosts[i];
            } else {
                data += hosts[i] + "\n";
            }
        }
    }

    sdfs_message sdfs_msg;
    sdfs_msg.set_type_mn_ls(data);

    if (sdfs_utils::send_message(server.get(), socket, sdfs_msg) == SDFS_FAILURE) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

int sdfs_master_impl::rep_operation(int socket, std::string hostname, std::string sdfs_filename) {
    // SENDING REP REQUEST
    lg->info("master and is now sending a rep for " + sdfs_filename + " to " + hostname);

    // CREATE REP REQUEST MESSAGE
    sdfs_message sdfs_msg;
    sdfs_msg.set_type_rep(hostname, sdfs_filename);

    // SEND THE REQ REQUEST OVER THE SOCKET
    if (sdfs_utils::send_message(server.get(), socket, sdfs_msg) == SDFS_FAILURE) return SDFS_FAILURE;

    // RECEIVE REPLICATING NODE COMMAND STATUS MESSAGE
    sdfs_message response;
    if (sdfs_utils::receive_message(server.get(), socket, &response) == SDFS_FAILURE) return SDFS_FAILURE;
    if (response.get_type() == sdfs_message::msg_type::success) {
        // UPDATE RECORDS AND RETURN
        file_to_hostnames[sdfs_filename].push_back(hostname);
        hostname_to_files[hostname].push_back(sdfs_filename);
        return SDFS_SUCCESS;
    } else if (response.get_type() == sdfs_message::msg_type::fail) {
        return SDFS_FAILURE;
    } else {
        return SDFS_FAILURE;
    }

    return SDFS_SUCCESS;
}

int sdfs_master_impl::files_operation(int socket, std::string hostname, std::string data) {
    std::stringstream ss(data);
    std::string filename;

    if (data != "") {
        while (std::getline(ss, filename, '\n')) {
            file_to_hostnames[filename].push_back(hostname);
            hostname_to_files[hostname].push_back(filename);
        }
    }

    return SDFS_SUCCESS;
}

bool sdfs_master_impl::sdfs_file_exists(std::string sdfs_filename) {
    return (file_to_hostnames.find(sdfs_filename) != file_to_hostnames.end() && file_to_hostnames[sdfs_filename].size() != 0);
}

std::vector<std::string> sdfs_master_impl::get_hostnames() {
    std::vector<member> members = hb->get_members();

    std::vector<unsigned int> indices(members.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::random_shuffle(indices.begin(), indices.end());

    std::vector<std::string> hostnames;
    for (unsigned i = 0; i < std::min((unsigned) members.size(), (unsigned) NUM_REPLICAS); i++) {
        hostnames.push_back(members[indices[i]].hostname);
    }

    return hostnames;
}

register_auto<sdfs_master, sdfs_master_impl> register_sdfs_master;
