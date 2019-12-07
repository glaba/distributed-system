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
    // @TODO: ALSO REMOVE ALL FILES HERE
    return;
}

void sdfs_server_impl::handle_connection(int socket) {
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
        } else if (request.get_type() == sdfs_message::msg_type::del) {
            del_operation(socket, sdfs_filename);
        } else if (request.get_type() == sdfs_message::msg_type::ls) {
            ls_operation(socket, sdfs_filename);
        } else if (request.get_type() == sdfs_message::msg_type::rep) {
            std::string sdfs_hostname = request.get_sdfs_hostname();
            rep_operation(socket, sdfs_hostname, sdfs_filename);
        }
    }

    // cleanup
    server->close_connection(socket);
}

int sdfs_server_impl::put_operation(int socket, std::string sdfs_filename) {
    // the client has made a put request to master
    // and the master has approved the client request
    lg->info("server received request from client to put " + sdfs_filename);

    // RECEIVE THE FILE FROM THE CLIENT
    if (sdfs_utils::read_file_from_socket(server.get(), socket, sdfs_filename) == -1) return SDFS_FAILURE;

    // @TODO: ADD FUNCTIONALITY OF UPDATING A LIST OF FILES
    return SDFS_SUCCESS;
}

int sdfs_server_impl::get_operation(int socket, std::string sdfs_filename) {
    // the client has made a get request to master
    // and the master has approved the client request
    lg->info("server received request from client to get " + sdfs_filename);

    // RECEIVE THE FILE FROM THE CLIENT
    if (sdfs_utils::write_file_to_socket(server.get(), socket, sdfs_filename) == -1) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

int sdfs_server_impl::del_operation(int socket, std::string sdfs_filename) {
    // the client has made a dele request to master
    // and the master has approved the client request
    lg->info("server received request from client to get " + sdfs_filename);

    if (del_file(sdfs_filename) == -1) return SDFS_FAILURE;

    // @TODO: ADD FUNCTIONALITY OF UPDATING A LIST OF FILES (or just use a store-like op)
    return SDFS_SUCCESS;
}

int sdfs_server_impl::ls_operation(int socket, std::string sdfs_filename) {
    // the client has made a del request to master
    // and the master has approved the client request
    lg->info("server received request from client to ls " + sdfs_filename);

    // @TODO: ADD FILE EXISTS / NOT EXISTS MESSAGE (or just use a generic failure / success)
    bool exists = file_exists(sdfs_filename);

    // SEND RESPONSE OVER SOCKET
    return SDFS_SUCCESS;
}

int sdfs_server_impl::rep_operation(int socket, std::string sdfs_hostname, std::string sdfs_filename) {
    // the master has made a rep request to the server
    lg->info("server received request from master to rep " + sdfs_filename + " to host " + sdfs_hostname);

    // GET INTERNAL SOCKET TO HOSTNAME
    // @TODO: WHAT HAPPENS IF I FAIL - WILL THE MASTER INDEFINITELY WAIT FOR A RESPONSE?
    int internal_socket;
    if ((internal_socket = client->setup_connection(sdfs_hostname, config->get_sdfs_internal_port()))== -1) return SDFS_FAILURE;

    std::string local_filename = config->get_sdfs_dir() + sdfs_filename;

    // SEND THE PUT REQUEST AND THEN SEND THE FILE
    sdfs_message put_msg;
    put_msg.set_type_put(sdfs_filename);
    if (sdfs_utils::send_message(client.get(), internal_socket, put_msg) == SDFS_FAILURE) return SDFS_FAILURE;
    if (sdfs_utils::write_file_to_socket(client.get(), internal_socket, local_filename) == -1) return SDFS_FAILURE;

    return SDFS_SUCCESS;
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

register_auto<sdfs_server, sdfs_server_impl> register_sdfs_server;
