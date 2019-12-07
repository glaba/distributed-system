#include "sdfs_master.hpp"

sdfs_master_impl::sdfs_master_impl(environment &env)
    : el(env.get<election>()),
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
    return;
}

void sdfs_master_impl::stop() {
    // @TODO: ADD LOGIC TO STOP ACCEPTING CONNECTIONS
    // @TODO: THIS SHOULD PROBABLY BE AS A CALLBACK FROM ELECTION
    return;
}

int sdfs_master_impl::put_operation(int socket, std::string sdfs_filename) {
    // RECEIVED PUT REQUEST FROM CLIENT
    lg->info("master received client put request for " + sdfs_filename);

    // @TODO: GET THIS HOSTNAME SOMEHOW USING SDFS_FILENAME (hash)
    std::string hostname;

    // RESPOND WITH APPROPRIATE LOCATION FOR CLIENT TO FORWARD REQUEST TO
    sdfs_message sdfs_msg;
    sdfs_msg.set_type_mn_put(hostname);
    if (sdfs_utils::send_message(server.get(), socket, sdfs_msg) == SDFS_FAILURE) return SDFS_FAILURE;

    // RECEIVE CLIENT COMMAND STATUS MESSAGE
    sdfs_message response;
    if (sdfs_utils::receive_message(server.get(), socket, &response) == SDFS_FAILURE) return SDFS_FAILURE;

    if (response.get_type() == sdfs_message::msg_type::fail) {
        return SDFS_FAILURE;
    } else if (response.get_type() == sdfs_message::msg_type::success) {
        // @TODO: INSERT REPLICATION HERE VIA SENDING REQUESTS
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
    if (sdfs_file_exists(sdfs_filename)) {
        // file does not exist - send msg with "" hostname and return
        sdfs_message empty_host;
        empty_host.set_type_mn_get("");
        if (sdfs_utils::send_message(server.get(), socket, empty_host) == SDFS_FAILURE) return SDFS_FAILURE;
        return SDFS_SUCCESS;
    } else {
        // file exists
        hostname = (file_to_hostnames->find(sdfs_filename)->second)[0];
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

    // GET LIST OF NODES WITH THE FILE
    sdfs_message sdfs_msg;
    // sdfs_msg.set_type_mn_del(hostname);

    // @TODO: DETERMINE HOW I WILL SEND THE RESPECTIVE NODES
    //        THE DELETE MESSAGE FROM THE MASTER (maybe copy pasta client tbh)

    // if (sdfs_utils::send_message(server.get(), socket, sdfs_msg) == SDFS_FAILURE) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

int sdfs_master_impl::ls_operation(int socket, std::string sdfs_filename) {
    // RECEIVED LS REQUEST FROM CLIENT
    lg->info("master received client ls request for " + sdfs_filename);

    std::string data = "";
    // GET LIST OF NODES WITH THE FILE
    if (sdfs_file_exists(sdfs_filename)) {
        std::vector<std::string> hosts = file_to_hostnames->find(sdfs_filename)->second;
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

    // @TODO: probably fine not to take a client response here, there's not point

    return SDFS_SUCCESS;
}

bool sdfs_master_impl::sdfs_file_exists(std::string sdfs_filename) {
    return file_to_hostnames->find(sdfs_filename) == file_to_hostnames->end();
}

register_auto<sdfs_master, sdfs_master_impl> register_sdfs_master;
