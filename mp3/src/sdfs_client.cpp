#include "sdfs_client.h"
#include "sdfs_client.hpp"

#include <random>

sdfs_client_impl::sdfs_client_impl(environment &env)
    : el(env.get<election>()),
      lg(env.get<logger_factory>()->get_logger("sdfs_client")),
      client(env.get<tcp_factory>()->get_tcp_client()),
      server(env.get<tcp_factory>()->get_tcp_server()),
      config(env.get<configuration>()),
      mt(std::chrono::system_clock::now().time_since_epoch().count())
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

int sdfs_client_impl::put_operation(std::string local_filename, std::string sdfs_filename) {
    int master_socket;
    if ((master_socket = sdfs_client_impl::get_master_socket()) == -1) return SDFS_FAILURE;

    // master node correspondence to get the internal hostname
    std::string hostname;
    hostname = put_operation_master(master_socket, local_filename, sdfs_filename);

    int internal_socket;
    internal_socket = sdfs_client_impl::get_internal_socket(hostname);

    // internal node correspondence to put the file
    int ret = put_operation_internal(internal_socket, local_filename, sdfs_filename);
    client->close_connection(internal_socket);

    // report back to master with status (remove above close connection too)
    // create success/fail message
    sdfs_message res;
    if (ret == SDFS_SUCCESS) {
        res.set_type_success();
    } else {
        res.set_type_fail();
    }

    if (sdfs_utils::send_message(client.get(), master_socket, res) == SDFS_FAILURE) {
        client->close_connection(master_socket);
        return SDFS_FAILURE;
    }

    client->close_connection(master_socket);

    return ret;
}

int sdfs_client_impl::get_sharded(std::string local_filename, std::string sdfs_filename_prefix) {
    std::string id = std::to_string(mt());
    std::string temp_prefix = config->get_mj_dir() + "shard" + id;

    unsigned num_shards = 0;
    while (true) {
        std::string temp = temp_prefix + "_" + std::to_string(num_shards);
        if (get_operation(temp, sdfs_filename_prefix + "." + std::to_string(num_shards)) != 0) {
            break;
        }
        num_shards++;
    }
    std::string cat_command = "cat ";
    std::string rm_command = "rm ";
    for (unsigned i = 0; i < num_shards; i++) {
        cat_command += "\"" + temp_prefix + "_" + std::to_string(i) + "\" ";
        rm_command += "\"" + temp_prefix + "_" + std::to_string(i) + "\" ";
    }
    cat_command += "> \"" + local_filename + "\"";

    FILE *stream = popen(cat_command.c_str(), "r");
    if (pclose(stream)) {
        return -1;
    }
    stream = popen(rm_command.c_str(), "r");
    pclose(stream);

    return 0;
}

int sdfs_client_impl::get_operation(std::string local_filename, std::string sdfs_filename) {
    int master_socket;
    if ((master_socket = sdfs_client_impl::get_master_socket()) == -1) return SDFS_FAILURE;

    // master node correspondence to get the internal hostname
    std::string hostname;
    hostname = get_operation_master(master_socket, local_filename, sdfs_filename);

    if (hostname == "") {
        client->close_connection(master_socket);
        return SDFS_FAILURE;
    }

    int internal_socket;
    if ((internal_socket = sdfs_client_impl::get_internal_socket(hostname)) == -1) {
        client->close_connection(master_socket);
        return SDFS_FAILURE;
    }

    // internal node correspondence to put the file
    int ret = get_operation_internal(internal_socket, local_filename, sdfs_filename);
    client->close_connection(internal_socket);

    // report back to master with status (remove above close connection too)
    // create success/fail message
    sdfs_message res;
    if (ret == SDFS_SUCCESS) {
        res.set_type_success();
    } else {
        res.set_type_fail();
    }

    if (sdfs_utils::send_message(client.get(), master_socket, res) == SDFS_FAILURE) {
        client->close_connection(master_socket);
        return SDFS_FAILURE;
    }
    client->close_connection(master_socket);

    return ret;
}

std::string sdfs_client_impl::get_metadata_operation(std::string sdfs_filename) {
    int master_socket;
    if ((master_socket = sdfs_client_impl::get_master_socket()) == -1) return "";

    // master node correspondence to get the internal hostname
    std::string hostname;
    hostname = get_metadata_operation_master(master_socket, sdfs_filename);

    int internal_socket;
    if ((internal_socket = sdfs_client_impl::get_internal_socket(hostname)) == -1) return "";

    // internal node correspondence to get the file metadata
    std::string ret = get_metadata_operation_internal(internal_socket, sdfs_filename);
    client->close_connection(internal_socket);

    // report back to master with status
    // create success/fail message
    sdfs_message res;
    if (ret != "") {
        res.set_type_success();
    } else {
        res.set_type_fail();
    }

    if (sdfs_utils::send_message(client.get(), master_socket, res) == SDFS_FAILURE) return "";
    client->close_connection(master_socket);

    return ret;
}

int sdfs_client_impl::del_operation(std::string sdfs_filename) {
    int master_socket;
    if ((master_socket = sdfs_client_impl::get_master_socket()) == -1) return SDFS_FAILURE;

    // master node correspondence del files
    int status = del_operation_master(master_socket, sdfs_filename);
    client->close_connection(master_socket);

    return status;
}

int sdfs_client_impl::ls_operation(std::string sdfs_filename) {
    int master_socket;
    if ((master_socket = sdfs_client_impl::get_master_socket()) == -1) return SDFS_FAILURE;

    // master node correspondence del files
    // @TODO: either print in the master function or relay std::string results here and print
    int status = ls_operation_master(master_socket, sdfs_filename);
    client->close_connection(master_socket);

    return status;
}

int sdfs_client_impl::append_operation(std::string local_filename, std::string sdfs_filename) {
    int master_socket;
    if ((master_socket = sdfs_client_impl::get_master_socket()) == -1) return SDFS_FAILURE;

    // master node correspondence to get the internal hostname
    std::string metadata = "test md";   // @TODO: get the metadata
    std::vector<std::string> res = append_operation_master(master_socket, metadata, local_filename, sdfs_filename);
    std::string hostname = "";
    std::string filename = "";
    if (res.size() == 2) {
        hostname = res[0];
        filename = res[1];
    }

    int internal_socket;
    internal_socket = sdfs_client_impl::get_internal_socket(hostname);

    // internal node correspondence to put the file
    int ret = put_operation_internal(internal_socket, local_filename, filename);
    client->close_connection(internal_socket);

    // report back to master with status (remove above close connection too)
    // create success/fail message
    sdfs_message res_msg;
    if (ret == SDFS_SUCCESS) {
        res_msg.set_type_success();
    } else {
        res_msg.set_type_fail();
    }

    if (sdfs_utils::send_message(client.get(), master_socket, res_msg) == SDFS_FAILURE) {
        client->close_connection(master_socket);
        return SDFS_FAILURE;
    }
    client->close_connection(master_socket);

    return ret;
}

int sdfs_client_impl::get_index_operation(std::string sdfs_filename) {
    int master_socket;
    if ((master_socket = sdfs_client_impl::get_master_socket()) == -1) return SDFS_FAILURE;

    // master node correspondence to get the internal hostname
    std::string index = get_index_operation_master(master_socket, sdfs_filename);
    client->close_connection(master_socket);

    if (index == "") return SDFS_FAILURE;

    std::string::size_type sz;

    return std::stoi(index, &sz);
}

int sdfs_client_impl::store_operation() {
    // local operation to ls sdfs directory
    DIR *dirp = opendir(config->get_sdfs_dir().c_str());
    struct dirent *dp;

    while ((dp = readdir(dirp)) != NULL) {
        std::cout << dp->d_name << std::endl;;
    }

    closedir(dirp);
    return SDFS_SUCCESS;
}

std::string sdfs_client_impl::put_operation_master(int socket, std::string local_filename, std::string sdfs_filename) {
    // SOCKET IS WITH MASTER
    lg->debug("client is sending request to put " + sdfs_filename + " to master");

    // create put request message
    sdfs_message put_req;
    put_req.set_type_put(sdfs_filename);

    // send request message to master
    if (sdfs_utils::send_message(client.get(), socket, put_req) == SDFS_FAILURE) return "";

    // receive master node put response
    sdfs_message mn_response;
    if (sdfs_utils::receive_message(client.get(), socket, &mn_response) == SDFS_FAILURE) return "";
    if (mn_response.get_type() != sdfs_message::msg_type::mn_put) return "";

    return mn_response.get_sdfs_hostname();
}

std::string sdfs_client_impl::get_operation_master(int socket, std::string local_filename, std::string sdfs_filename) {
    // SOCKET IS WITH MASTER
    lg->debug("client is sending request to get " + sdfs_filename + " to master");

    // create get request message
    sdfs_message get_req;
    get_req.set_type_get(sdfs_filename);

    // send request message to master
    if (sdfs_utils::send_message(client.get(), socket, get_req) == SDFS_FAILURE) return "";

    // receive master node put response
    sdfs_message mn_response;
    if (sdfs_utils::receive_message(client.get(), socket, &mn_response) == SDFS_FAILURE) return "";
    if (mn_response.get_type() != sdfs_message::msg_type::mn_get) return "";

    return mn_response.get_sdfs_hostname();
}

std::string sdfs_client_impl::get_metadata_operation_master(int socket, std::string sdfs_filename) {
    // SOCKET IS WITH MASTER
    lg->debug("client is sending request to get metadata for " + sdfs_filename + " to master");

    // create get request message
    sdfs_message gmd_req;
    gmd_req.set_type_gmd(sdfs_filename);

    // send request message to master
    if (sdfs_utils::send_message(client.get(), socket, gmd_req) == SDFS_FAILURE) return "";

    // receive master node put response (will be same as get response for reusability)
    sdfs_message mn_response;
    if (sdfs_utils::receive_message(client.get(), socket, &mn_response) == SDFS_FAILURE) return "";
    if (mn_response.get_type() != sdfs_message::msg_type::mn_get) return "";

    return mn_response.get_sdfs_hostname();
}

std::vector<std::string> sdfs_client_impl::append_operation_master(int socket, std::string metadata, std::string local_filename, std::string sdfs_filename) {
    // SOCKET IS WITH MASTER
    lg->debug("client is sending request to append for " + sdfs_filename + " to master");

    std::vector<std::string> ret;
    // create append request message
    sdfs_message append_req;
    append_req.set_type_append(sdfs_filename, metadata);

    // send request message to master
    if (sdfs_utils::send_message(client.get(), socket, append_req) == SDFS_FAILURE) return ret;

    // receive master node put response (will be same as get response for reusability)
    sdfs_message mn_response;
    if (sdfs_utils::receive_message(client.get(), socket, &mn_response) == SDFS_FAILURE) return ret;
    if (mn_response.get_type() != sdfs_message::msg_type::mn_append) return ret;

    ret.push_back(mn_response.get_sdfs_hostname());
    ret.push_back(mn_response.get_sdfs_filename());
    return ret;
}

int sdfs_client_impl::del_operation_master(int socket, std::string sdfs_filename) {
    // SOCKET IS WITH MASTER
    lg->debug("client is sending request to del " + sdfs_filename + " to master");

    // create del request message
    sdfs_message del_req;
    del_req.set_type_del(sdfs_filename);

    // send request message to master
    if (sdfs_utils::send_message(client.get(), socket, del_req) == SDFS_FAILURE) return SDFS_FAILURE;

    // receive master node del response
    sdfs_message mn_response;
    if (sdfs_utils::receive_message(client.get(), socket, &mn_response) == SDFS_FAILURE) return SDFS_FAILURE;
    if (mn_response.get_type() != sdfs_message::msg_type::success) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

int sdfs_client_impl::ls_operation_master(int socket, std::string sdfs_filename) {
    // SOCKET IS WITH MASTER
    lg->debug("client is sending request to ls " + sdfs_filename + " to master");

    // create ls request message
    sdfs_message ls_req;
    ls_req.set_type_ls(sdfs_filename);

    // send request message to master
    if (sdfs_utils::send_message(client.get(), socket, ls_req) == SDFS_FAILURE) return SDFS_FAILURE;

    // receive master node ls response
    sdfs_message mn_response;
    if (sdfs_utils::receive_message(client.get(), socket, &mn_response) == SDFS_FAILURE) return SDFS_FAILURE;
    if (mn_response.get_type() != sdfs_message::msg_type::mn_ls) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

std::string sdfs_client_impl::get_index_operation_master(int socket, std::string sdfs_filename) {
    // SOCKET IS WITH MASTER
    lg->info("client is sending request to get index " + sdfs_filename + " to master");

    // create gidx request message
    sdfs_message gidx_req;
    gidx_req.set_type_gidx(sdfs_filename);

    // send request message to master
    if (sdfs_utils::send_message(client.get(), socket, gidx_req) == SDFS_FAILURE) return "";

    // receive master node ls response
    sdfs_message mn_response;
    if (sdfs_utils::receive_message(client.get(), socket, &mn_response) == SDFS_FAILURE) return "";
    if (mn_response.get_type() != sdfs_message::msg_type::mn_gidx) return "";

    return mn_response.get_data();

}

int sdfs_client_impl::put_operation_internal(int socket, std::string local_filename, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS DONE
    lg->debug("client is requesting to put " + sdfs_filename);

    // SEND THE PUT REQUEST AND THEN SEND THE FILE
    sdfs_message put_msg; put_msg.set_type_put(sdfs_filename);
    if (sdfs_utils::send_message(client.get(), socket, put_msg) == SDFS_FAILURE) return SDFS_FAILURE;
    if (sdfs_utils::write_file_to_socket(client.get(), socket, local_filename) == -1) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

int sdfs_client_impl::get_operation_internal(int socket, std::string local_filename, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS DONE
    lg->debug("client is requesting to get " + sdfs_filename + " as " + local_filename);

    // SEND THE GET REQUEST AND THEN RECEIVE THE FILE
    sdfs_message get_msg; get_msg.set_type_get(sdfs_filename);
    if (sdfs_utils::send_message(client.get(), socket, get_msg) == SDFS_FAILURE) return SDFS_FAILURE;
    if (sdfs_utils::read_file_from_socket(client.get(), socket, local_filename) == -1) return SDFS_FAILURE;

    return SDFS_SUCCESS;
}

std::string sdfs_client_impl::get_metadata_operation_internal(int socket, std::string sdfs_filename) {
    // MASTER CORRESPONDENCE IS DONE
    lg->debug("client is requesting to get metadata for " + sdfs_filename);

    std::string metadata;
    // SEND THE GET REQUEST AND THEN RECEIVE THE FILE
    sdfs_message gmd_msg; gmd_msg.set_type_gmd(sdfs_filename);
    if (sdfs_utils::send_message(client.get(), socket, gmd_msg) == SDFS_FAILURE) return "";
    if ((metadata = client->read_from_server(socket)) == "") return "";

    return metadata;
}

void sdfs_client_impl::send_files() {
    // to be called when master fails and new master is elected
    std::string files_list = "";
    // local operation to ls sdfs directory
    DIR *dirp = opendir(config->get_sdfs_dir().c_str());
    struct dirent *dp;

    while ((dp = readdir(dirp)) != NULL) {
        files_list += dp->d_name;
        files_list += "\n";
    }
    closedir(dirp);

    int master_socket = get_master_socket();
    sdfs_message files_msg;
    files_msg.set_type_files(config->get_hostname(), files_list);
    sdfs_utils::send_message(client.get(), master_socket, files_msg);
}

int sdfs_client_impl::get_master_socket() {
    if (mn_hostname != "") {
        return client->setup_connection(mn_hostname, config->get_sdfs_master_port());
    }

    // virtual int setup_connection(std::string host, int port) = 0;
    int socket = -1;
    el->wait_master_node([&] (member master_node) mutable {
        socket = client->setup_connection(master_node.hostname, config->get_sdfs_master_port());
    });
    return socket;
}

int sdfs_client_impl::get_internal_socket(std::string hostname) {
    int socket = client->setup_connection(hostname, config->get_sdfs_internal_port());
    return socket;
}

register_auto<sdfs_client, sdfs_client_impl> register_sdfs_client;
