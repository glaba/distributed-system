#include "sdfs_client.h"
#include "sdfs_client.hpp"

sdfs_client_impl::sdfs_client_impl(environment &env)
    : lg(env.get<logger_factory>()->get_logger("election")),
      client(env.get<tcp_factory>()->get_tcp_client()),
      server(env.get<tcp_factory>()->get_tcp_server()),
      config(env.get<configuration>())
{
    // @TODO: master node logic, probably going to need the election service as well
}

void sdfs_client_impl::start() {
    return;
}

void sdfs_client_impl::stop() {
    return;
}

int sdfs_client_impl::put_operation(int socket, std::string local_filename, std::string sdfs_filename) {
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
