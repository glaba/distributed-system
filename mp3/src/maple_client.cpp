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
    do {
        // TODO: put maple_exe in SDFS

        maple_message msg(maple_start_job{maple_exe, num_maples, sdfs_intermediate_filename_prefix, sdfs_src_dir});

        // Send the data to the master node
        int fd = client->setup_connection(maple_master, config->get_maple_master_port());
        if (fd < 0 || client->write_to_server(fd, msg.serialize()) <= 0) {
            error = "Node at " + maple_master + " is not running Maplejuice, retry";
            return false;
        }

        // This response will either indicate that the server is not the master node or that the job is complete
        string response = client->read_from_server(fd);

        // Check if the connection was closed unexpectedly
        if (response.length() == 0) {
            // In this case, the master has failed and the job should continue
            // TODO: implement reconnecting to the new master and continuing the job
        }

        maple_message response_msg(response.c_str(), response.length());

        if (!response_msg.is_well_formed()) {
            lg->info("Received invalid response from Maple master");
            error = "Maple master node is behaving unexpectedly";
            return false;
        }

        // If the node we connected to was not the master, it will tell us who is
        if (response_msg.get_msg_type() == maple_message::maple_msg_type::NOT_MASTER) {
            client->close_connection(fd);

            string master_node = response_msg.get_msg_data<maple_not_master>().master_node;
            // If there is no master node, retry in a couple seconds
            if (master_node == "") {
                lg->info("Master node is currently being re-elected, querying again in 5 seconds");
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                continue;
            } else {
                lg->info("Contacted node was not the master, but told us that the master is at " + master_node);
                run_job(master_node, maple_exe, num_maples, sdfs_intermediate_filename_prefix, sdfs_src_dir);
            }
        } else if (response_msg.get_msg_type() == maple_message::maple_msg_type::JOB_END) {
            if (response_msg.get_msg_data<maple_job_end>().succeeded) {
                lg->info("Maple job completed successfully");
                return true;
            } else {
                lg->info("Maple job failed to run successfully");
                error = "Provided maple_exe failed to run correctly on Maple nodes";
                return false;
            }
        }
    } while (true);
}

register_auto<maple_client, maple_client_impl> register_maple_client;
