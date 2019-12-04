#include "sdfs_client.h"
#include "sdfs_client.hpp"

sdfs_client_impl::sdfs_client_impl(environment &env)
    : el(env.get<election>()),
      lg(env.get<logger_factory>()->get_logger("sdfs_client")),
      client(env.get<tcp_factory>()->get_tcp_client()),
      server(env.get<tcp_factory>()->get_tcp_server()),
      config(env.get<configuration>())
{
    // @TODO: master node logic, probably going to need the election service as well
}

void sdfs_client_impl::start() {
    // @TODO: ADD LOGIC TO ACCEPT CLI INPUT AND CALL COMMANDS
    return;
}

void sdfs_client_impl::stop() {
    // @TODO: ADD LOGIC TO STOP ACCEPTING CLI INPUT
    return;
}

int sdfs_client_impl::put_operation(int socket, std::string local_filename, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS DONE
    lg->info("client is requesting to put " + sdfs_filename);

    // SEND THE PUT REQUEST AND THEN SEND THE FILE
    sdfs_message put_msg; put_msg.set_type_put(sdfs_filename);
    if (send_request(socket, put_msg) == -1) return SDFS_CLIENT_FAILURE;
    if (client->write_file_to_socket(socket, local_filename) == -1) return SDFS_CLIENT_FAILURE;

    // @TODO: determine if i want to use success messages from the node to the client
    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::get_operation(int socket, std::string local_filename, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS DONE
    lg->info("client is requesting to get " + sdfs_filename + " as " + local_filename);

    // SEND THE GET REQUEST AND THEN RECEIVE THE FILE
    sdfs_message get_msg; get_msg.set_type_get(sdfs_filename);
    if (send_request(socket, get_msg) == -1) return SDFS_CLIENT_FAILURE;
    if (client->read_file_from_socket(socket, local_filename) == -1) return SDFS_CLIENT_FAILURE;

    // @TODO: determine if i want to use success messages from the node to the client
    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::del_operation(int socket, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS NOT DONE - socket IS TO MASTER NODE
    lg->info("client is requesting to del " + sdfs_filename);

    // RECEIVE THE SUCCESS/FAIL RESPONSE FROM THE MASTER NODE
    sdfs_message sdfs_msg;
    receive_response(socket, &sdfs_msg);

    if (sdfs_msg.get_type() != sdfs_message::msg_type::success) return SDFS_CLIENT_FAILURE;

    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::ls_operation(int socket, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS NOT DONE - socket IS TO MASTER NODE
    lg->info("client is requesting to get " + sdfs_filename);

    // RECEIVE THE LS RESPONSE FROM THE MASTER NODE
    sdfs_message sdfs_msg;
    receive_response(socket, &sdfs_msg);

    if (sdfs_msg.get_type() != sdfs_message::msg_type::mn_ls) return SDFS_CLIENT_FAILURE;

    // @TODO: manage the response of the ls operation here (sdfs_msg.get_data())

    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::store_operation() {
    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::send_request(int socket, sdfs_message sdfs_msg) {
    lg->trace("client is sending request of type " + sdfs_msg.get_type_as_string());

    std::string msg = sdfs_msg.serialize();
    return client->write_to_server(socket, msg);
}

int sdfs_client_impl::receive_response(int socket, sdfs_message *sdfs_msg) {
    // receive response over socket
    std::string response;
    if ((response = client->read_from_server(socket)) == "") return SDFS_CLIENT_FAILURE;

    // determine if response was valid sdfs_message
    char response_cstr[response.length() + 1];
    strncpy(response_cstr, response.c_str(), response.length() + 1);
    *sdfs_msg = sdfs_message(response_cstr, strlen(response_cstr));
    if (sdfs_msg->get_type() == sdfs_message::msg_type::empty) return SDFS_CLIENT_FAILURE;

    lg->trace("client received response of type " + sdfs_msg->get_type_as_string());

    return SDFS_CLIENT_SUCCESS;
}

register_auto<sdfs_client, sdfs_client_impl> register_sdfs_client;
