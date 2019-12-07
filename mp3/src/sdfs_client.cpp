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
    if (sdfs_utils::send_message(client.get(), socket, put_msg) == SDFS_FAILURE) return SDFS_FAILURE;
    if (sdfs_utils::write_file_to_socket(client.get(), socket, local_filename) == -1) return SDFS_FAILURE;

    // @TODO: determine if i want to use success messages from the node to the client
    return SDFS_SUCCESS;
}

int sdfs_client_impl::get_operation(int socket, std::string local_filename, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS DONE
    lg->info("client is requesting to get " + sdfs_filename + " as " + local_filename);

    // SEND THE GET REQUEST AND THEN RECEIVE THE FILE
    sdfs_message get_msg; get_msg.set_type_get(sdfs_filename);
    if (sdfs_utils::send_message(client.get(), socket, get_msg) == SDFS_FAILURE) return SDFS_FAILURE;
    if (sdfs_utils::read_file_from_socket(client.get(), socket, local_filename) == -1) return SDFS_FAILURE;

    // @TODO: determine if i want to use success messages from the node to the client
    return SDFS_SUCCESS;
}

int sdfs_client_impl::del_operation(int socket, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS NOT DONE - socket IS TO MASTER NODE
    lg->info("client is requesting to del " + sdfs_filename);

    // RECEIVE THE SUCCESS/FAIL RESPONSE FROM THE MASTER NODE
    sdfs_message sdfs_msg;
    if (sdfs_utils::receive_message(client.get(), socket, &sdfs_msg) == SDFS_FAILURE) return SDFS_FAILURE;

    if (sdfs_msg.get_type() != sdfs_message::msg_type::success) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

int sdfs_client_impl::ls_operation(int socket, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS NOT DONE - socket IS TO MASTER NODE
    lg->info("client is requesting to get " + sdfs_filename);

    // RECEIVE THE LS RESPONSE FROM THE MASTER NODE
    sdfs_message sdfs_msg;
    if (sdfs_utils::receive_message(client.get(), socket, &sdfs_msg) == SDFS_FAILURE) return SDFS_FAILURE;

    if (sdfs_msg.get_type() != sdfs_message::msg_type::mn_ls) return SDFS_FAILURE;

    // @TODO: manage the response of the ls operation here (sdfs_msg.get_data())

    return SDFS_SUCCESS;
}

int sdfs_client_impl::store_operation() {
    return SDFS_SUCCESS;
}

register_auto<sdfs_client, sdfs_client_impl> register_sdfs_client;
