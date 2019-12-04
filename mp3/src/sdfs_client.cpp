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
    // SEND PUT REQUEST TO THE MASTER NODE AND RECEIVE RESPONSE

    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::get_operation(int socket, std::string local_filename, std::string sdfs_filename) {
    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::del_operation(int socket, std::string sdfs_filename) {
    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::ls_operation(int socket, std::string sdfs_filename) {
    return SDFS_CLIENT_SUCCESS;
}

int sdfs_client_impl::store_operation() {
    return SDFS_CLIENT_SUCCESS;
}

register_auto<sdfs_client, sdfs_client_impl> register_sdfs_client;
