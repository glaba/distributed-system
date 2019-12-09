#include "maple_client.h"
#include "maple_client.hpp"
#include "environment.h"
#include "mj_messages.h"
#include "partitioner.h"
#include "outputter.h"

using std::string;

maple_client_impl::maple_client_impl(environment &env)
    : sdfsc(env.get<sdfs_client>())
    , config(env.get<configuration>())
    , fac(env.get<tcp_factory>())
    , lg(env.get<logger_factory>()->get_logger("maple_client")) {}

std::string maple_client_impl::get_error() {
    return error;
}

bool maple_client_impl::run_job(string mj_node, string local_exe, string maple_exe, int num_maples,
    string sdfs_intermediate_filename_prefix, string sdfs_src_dir)
{
    do {
        sdfsc->put_operation(local_exe, maple_exe);

        mj_message msg(0, mj_start_job{maple_exe, num_maples, partitioner::type::round_robin,
            sdfs_src_dir, outputter::type::maple, sdfs_intermediate_filename_prefix});

        // Send the data to the node
        std::unique_ptr<tcp_client> client = fac->get_tcp_client();
        int fd = client->setup_connection(mj_node, config->get_mj_master_port());
        if (fd < 0 || client->write_to_server(fd, msg.serialize()) <= 0) {
            error = "Node at " + mj_node + " is not running Maplejuice, retry";
            return false;
        }

        // This response will either indicate that the server is not the master node or that the job is complete
        string response = client->read_from_server(fd);
        client->close_connection(fd);

        // Check if the connection was closed unexpectedly during the read
        if (response.length() == 0) {
            // In this case, the master has failed and the job should continue
            // TODO: implement reconnecting to the new master and continuing the job
        }

        mj_message response_msg(response.c_str(), response.length());

        if (!response_msg.is_well_formed()) {
            lg->info("Received invalid response from Maple master");
            error = "MapleJuice master node is behaving unexpectedly";
            return false;
        }

        // If the node we connected to was not the master, it will tell us who is
        if (response_msg.get_msg_type() == mj_message::mj_msg_type::NOT_MASTER) {
            string master_node = response_msg.get_msg_data<mj_not_master>().master_node;
            // If there is no master node, retry in a couple seconds
            if (master_node == "") {
                lg->info("Master node is currently being re-elected, querying again in 5 seconds");
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                continue;
            } else {
                lg->info("Contacted node was not the master, but told us that the master is at " + master_node);
                return run_job(master_node, local_exe, maple_exe, num_maples, sdfs_intermediate_filename_prefix, sdfs_src_dir);
            }
        } else if (response_msg.get_msg_type() == mj_message::mj_msg_type::JOB_END) {
            if (response_msg.get_msg_data<mj_job_end>().succeeded) {
                lg->info("Maple job completed successfully");
                return true;
            } else {
                lg->info("Maple job failed to run successfully");
                error = "Provided executable failed to run correctly on MapleJuice nodes";
                return false;
            }
        }
    } while (true);
}

register_auto<maple_client, maple_client_impl> register_maple_client;
