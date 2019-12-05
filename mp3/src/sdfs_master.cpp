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
    // @TODO: THIS FUNCTION SHOULD ALSO POPULATE THE HASH MAPS OF THE FILES

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
    if (send_message(socket, sdfs_msg) == -1) return SDFS_MASTER_FAILURE;

    // RECEIVE CLIENT COMMAND STATUS MESSAGE
    sdfs_message response;
    if (receive_message(socket, &response) == SDFS_MASTER_FAILURE) return SDFS_MASTER_FAILURE;
    if (response.get_type() == sdfs_message::msg_type::fail) {
        return SDFS_MASTER_FAILURE;
    } else if (response.get_type() == sdfs_message::msg_type::success) {
        // @TODO: INSERT REPLICATION HERE VIA SENDING REQUESTS
        return SDFS_MASTER_SUCCESS;
    } else {
        return SDFS_MASTER_FAILURE;
    }

    return SDFS_MASTER_SUCCESS;
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
        if (send_message(socket, empty_host) == -1) return SDFS_MASTER_FAILURE;
        return SDFS_MASTER_SUCCESS;
    } else {
        // file exists
        hostname = (file_to_hostnames->find(sdfs_filename)->second)[0];
    }

    // RESPOND WITH APPROPRIATE LOCATION FOR CLIENT TO FORWARD REQUEST TO
    sdfs_message sdfs_msg;
    sdfs_msg.set_type_mn_get(hostname);

    if (send_message(socket, sdfs_msg) == -1) return SDFS_MASTER_FAILURE;

    // RECEIVE CLIENT COMMAND STATUS MESSAGE
    sdfs_message response;
    if (receive_message(socket, &response) == SDFS_MASTER_FAILURE) return SDFS_MASTER_FAILURE;
    if (response.get_type() == sdfs_message::msg_type::success) {
        return SDFS_MASTER_SUCCESS;
    } else if (response.get_type() == sdfs_message::msg_type::fail) {
        // @TODO: SHOULD I TRY AND REDIRECT THE CLIENT OR JUST LET THEM ERR
        return SDFS_MASTER_FAILURE;
    } else {
        return SDFS_MASTER_FAILURE;
    }

    return SDFS_MASTER_SUCCESS;
}

int sdfs_master_impl::del_operation(int socket, std::string sdfs_filename) {
    // RECEIVED DEL REQUEST FROM CLIENT
    lg->info("master received client del request for " + sdfs_filename);

    // GET LIST OF NODES WITH THE FILE
    sdfs_message sdfs_msg;
    // sdfs_msg.set_type_mn_del(hostname);

    // @TODO: DETERMINE HOW I WILL SEND THE RESPECTIVE NODES
    //        THE DELETE MESSAGE FROM THE MASTER (maybe copy pasta client tbh)

    // if (send_message(socket, sdfs_msg) == -1) return SDFS_MASTER_FAILURE;

    return SDFS_MASTER_SUCCESS;
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

    if (send_message(socket, sdfs_msg) == -1) return SDFS_MASTER_FAILURE;

    // @TODO: probably fine not to take a client response here, there's not point

    return SDFS_MASTER_SUCCESS;
}

int sdfs_master_impl::send_message(int socket, sdfs_message sdfs_msg) {
    lg->trace("master is sending message of type " + sdfs_msg.get_type_as_string());

    std::string msg = sdfs_msg.serialize();
    return server->write_to_client(socket, msg);
}

int sdfs_master_impl::receive_message(int socket, sdfs_message *sdfs_msg) {
    // receive response over socket
    std::string response;
    if ((response = server->read_from_client(socket)) == "") return SDFS_MASTER_FAILURE;

    // determine if response was valid sdfs_message
    char response_cstr[response.length() + 1];
    strncpy(response_cstr, response.c_str(), response.length() + 1);
    *sdfs_msg = sdfs_message(response_cstr, strlen(response_cstr));
    if (sdfs_msg->get_type() == sdfs_message::msg_type::empty) return SDFS_MASTER_FAILURE;

    lg->trace("master received response of type " + sdfs_msg->get_type_as_string());

    return SDFS_MASTER_SUCCESS;
}

bool sdfs_master_impl::sdfs_file_exists(std::string sdfs_filename) {
    return file_to_hostnames->find(sdfs_filename) == file_to_hostnames->end();
}

register_auto<sdfs_master, sdfs_master_impl> register_sdfs_master;
