#include "maple_client.h"
#include "maple_client.hpp"
#include "environment.h"
#include "maple_messages.h"

using std::string;

maple_client_impl::maple_client_impl(environment &env)
    : sdfsc(nullptr /* Until sdfs_client is implemented, we will leave this empty */)
    , config(env.get<configuration>())
    , client(env.get<tcp_factory>()->get_tcp_client())
    , lg(env.get<logger_factory>()->get_logger("maple_client")) {}

std::string maple_client_impl::get_error() {
    return error;
}

bool maple_client_impl::run_job(string maple_master, string maple_exe, int num_maples,
    string sdfs_intermediate_filename_prefix, string sdfs_src_dir)
{
    // TODO: put maple_exe into SDFS using sdfs_client

    maple_message msg;
    maple_start_job data;
    data.maple_exe = maple_exe;
    data.num_maples = num_maples;
    data.sdfs_intermediate_filename_prefix = sdfs_intermediate_filename_prefix;
    data.sdfs_src_dir = sdfs_src_dir;
    msg.set_msg_data(data);

    // Send the data to the master node
    int fd = client->setup_connection(maple_master, config->get_maple_port());
    if (fd < 0 || client->write_to_server(fd, msg.serialize()) <= 0) {
        error = "Specified node is not running";
        return false;
    }

    // Get the results of the job
    string response = client->read_from_server(fd);

    // Check if the connection was closed unexpectedly
    if (response.length() == 0) {
        // In this case, the master has failed and the job should continue
        // TODO: implement reconnecting to the new master and continuing the job
    }

    maple_message response_msg(response.c_str(), response.length());

    if (!response_msg.is_well_formed() || response_msg.get_msg_type() != maple_message::maple_msg_type::JOB_END) {
        lg->info("Received invalid response from Maple master");
        error = "Maple master node is behaving unexpectedly";
        return false;
    }

    if (response_msg.get_msg_data<maple_job_end>().succeeded) {
        lg->info("Maple job completed successfully");
        return true;
    } else {
        lg->info("Maple job failed to run successfully");
        error = "Provided maple_exe failed to run correctly on Maple nodes";
        return false;
    }
}

register_auto<maple_client, maple_client_impl> register_maple_client;
