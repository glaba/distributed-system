#include "sdfs_master.hpp"

sdfs_master_impl::sdfs_master_impl(environment &env)
    : lg(env.get<logger_factory>()->get_logger("election")),
      client(env.get<tcp_factory>()->get_tcp_client()),
      server(env.get<tcp_factory>()->get_tcp_server()),
      config(env.get<configuration>())
{

}

void sdfs_master_impl::start() {
    return;
}

void sdfs_master_impl::stop() {
    return;
}

int sdfs_master_impl::put_operation(int socket, std::string sdfs_filename) {
    return SDFS_MASTER_SUCCESS;
}

int sdfs_master_impl::get_operation(int socket, std::string sdfs_filename) {
    return SDFS_MASTER_SUCCESS;
}

int sdfs_master_impl::del_operation(int socket, std::string sdfs_filename) {
    return SDFS_MASTER_SUCCESS;
}

int sdfs_master_impl::ls_operation(int socket, std::string sdfs_filename) {
    return SDFS_MASTER_SUCCESS;
}
