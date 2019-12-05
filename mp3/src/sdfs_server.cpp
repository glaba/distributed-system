#include "sdfs_server.h"
#include "sdfs_server.hpp"


sdfs_server_impl::sdfs_server_impl(environment &env)
    : el(env.get<election>()),
      lg(env.get<logger_factory>()->get_logger("sdfs_server")),
      client(env.get<tcp_factory>()->get_tcp_client()),
      server(env.get<tcp_factory>()->get_tcp_server()),
      config(env.get<configuration>())
{
    // @TODO: ADD ELECTION SERVICE IN THE ABOVE
}

void sdfs_server_impl::start() {
    // @TODO: ADD LOGIC TO ACCEPT CONNECTIONS AND SPIN UP NEW THREADS
    return;
}

void sdfs_server_impl::stop() {
    // @TODO: ADD LOGIC TO STOP ACCEPTING CONNECTIONS
    return;
}

int sdfs_server_impl::put_operation(int socket, std::string sdfs_filename) {
    // the client has made a put request to master
    // and the master has approved the client request
    lg->info("server received request from client to put " + sdfs_filename);

    // @TODO: determine if i should use acks for put
    // RECEIVE THE FILE FROM THE CLIENT
    if (tcp_file_transfer::read_file_from_socket(server.get(), socket, sdfs_filename) == -1) return SDFS_SERVER_FAILURE;

    // @TODO: MAYBE ADD RESPONSE TO CLIENT (probably fine for client to use write response though)
    // @TODO: ADD FUNCTIONALITY OF UPDATING A LIST OF FILES
    return SDFS_SERVER_SUCCESS;
}

int sdfs_server_impl::get_operation(int socket, std::string sdfs_filename) {
    // the client has made a get request to master
    // and the master has approved the client request
    lg->info("server received request from client to get " + sdfs_filename);

    // @TODO: determine if i should use acks for get
    // RECEIVE THE FILE FROM THE CLIENT
    if (tcp_file_transfer::write_file_to_socket(server.get(), socket, sdfs_filename) == -1) return SDFS_SERVER_FAILURE;

    return SDFS_SERVER_SUCCESS;
}

int sdfs_server_impl::del_operation(int socket, std::string sdfs_filename) {
    // the client has made a dele request to master
    // and the master has approved the client request
    lg->info("server received request from client to get " + sdfs_filename);

    if (del_file(sdfs_filename) == -1) return SDFS_SERVER_FAILURE;

    // @TODO: ADD FUNCTIONALITY OF UPDATING A LIST OF FILES (or just use a store-like op)
    return SDFS_SERVER_SUCCESS;
}

int sdfs_server_impl::ls_operation(int socket, std::string sdfs_filename) {
    // the client has made a dele request to master
    // and the master has approved the client request
    lg->info("server received request from client to ls " + sdfs_filename);

    // @TODO: ADD FILE EXISTS / NOT EXISTS MESSAGE (or just use a generic failure / success)
    bool exists = file_exists(sdfs_filename);

    // SEND RESPONSE OVER SOCKET
    return SDFS_SERVER_SUCCESS;
}

bool sdfs_server_impl::file_exists(std::string sdfs_filename) {
    struct stat buffer;
    std::string full_path = config->get_sdfs_dir() + sdfs_filename;
    return (stat(full_path.c_str(), &buffer) == 0);
}

int sdfs_server_impl::del_file(std::string sdfs_filename) {
    std::string full_path = config->get_sdfs_dir() + sdfs_filename;
    return remove(full_path.c_str());
}

int sdfs_server_impl::send_client_ack(int socket) {
    lg->trace("server is sending ack to client");
    sdfs_message ack_message;
    ack_message.set_type_ack();
    std::string msg = ack_message.serialize();
    return server->write_to_client(socket, msg);
}

register_auto<sdfs_server, sdfs_server_impl> register_sdfs_server;
