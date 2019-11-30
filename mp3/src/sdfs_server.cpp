#include "sdfs_server.h"
#include "sdfs_server.hpp"


sdfs_server_impl::sdfs_server_impl(environment &env)
    : lg(env.get<logger_factory>()->get_logger("election")),
      client(env.get<tcp_factory>()->get_tcp_client()),
      server(env.get<tcp_factory>()->get_tcp_server()),
      config(env.get<configuration>())
{
    // @TODO: may or may not need more stuff here
}

void sdfs_server_impl::start() {
    return;
}

void sdfs_server_impl::stop() {
    return;
}

int sdfs_server_impl::put_operation(int socket, std::string sdfs_filename) {
    return SDFS_SERVER_SUCCESS;
}

int sdfs_server_impl::get_operation(int socket, std::string sdfs_filename) {
    return SDFS_SERVER_SUCCESS;
}

int sdfs_server_impl::del_operation(int socket, std::string sdfs_filename) {
    return SDFS_SERVER_SUCCESS;
}

int sdfs_server_impl::ls_operation(int socket, std::string sdfs_filename) {
    return SDFS_SERVER_SUCCESS;
}
