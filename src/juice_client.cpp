#include "juice_client.h"
#include "juice_client.hpp"
#include "environment.h"
#include "mj_messages.h"
#include "partitioner.h"
#include "processor.h"

using std::string;

juice_client_impl::juice_client_impl(environment &env)
    : sdfsc(env.get<sdfs_client>())
    , config(env.get<configuration>())
    , fac(env.get<tcp_factory>())
    , lg(env.get<logger_factory>()->get_logger("juice_client")) {}

auto juice_client_impl::get_error() const -> string {
    return error;
}

template <typename Msg>
auto juice_client_impl::filter_msg(std::string const& msg_str) -> std::optional<Msg> {
    Msg msg;
    if (msg.deserialize_from(msg_str)) {
        return msg;
    } else {
        return std::nullopt;
    }
}

auto juice_client_impl::run_job(string const& mj_node, string const& local_exe, string const& juice_exe,
    int num_juices, partitioner::type partitioner_type, string const& sdfs_src_dir, string const& sdfs_output_dir) -> bool
{
    do {
        lg->info("Starting job");
        sdfsc->set_master_node(mj_node);
        if (sdfsc->put(local_exe, juice_exe) != 0) {
            return false;
        }

        mj_message::start_job start_msg(0, juice_exe, num_juices, static_cast<uint32_t>(partitioner_type),
            static_cast<uint32_t>(processor::type::juice), sdfs_src_dir, sdfs_output_dir, 50, 4);

        // Send the data to the node
        std::unique_ptr<tcp_client> client = fac->get_tcp_client(mj_node, config->get_mj_master_port());
        if (client.get() == nullptr || client->write_to_server(start_msg.serialize()) <= 0) {
            error = "Node at " + mj_node + " is not running MapleJuice, retry";
            return false;
        }

        // This response will either indicate that the server is not the master node or that the job is complete
        string response = client->read_from_server();

        // Check if the connection was closed unexpectedly during the read
        if (response.length() == 0) {
            // In this case, the master has failed and the job should continue
            // TODO: implement reconnecting to the new master and continuing the job
        }

        mj_message::common_data common;
        if (!common.deserialize_from(string(response.c_str() + 4, response.length() - 4), true)) {
            lg->info("Received invalid response from Juice master");
            error = "MapleJuice master node sent invalid data";
            return false;
        }

        switch (common.type) {
            case static_cast<uint32_t>(mj_message::msg_type::not_master): {
                if (auto msg = filter_msg<mj_message::not_master>(response)) {
                    string master_node = msg.value().master_node;
                    // If there is no master node, retry in a couple seconds
                    if (master_node == "") {
                        lg->info("Master node is currently being re-elected, querying again in 5 seconds");
                        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                        continue;
                    } else {
                        lg->info("Contacted node was not the master, but told us that the master is at " + master_node);
                        return run_job(master_node, local_exe, juice_exe, num_juices, partitioner_type, sdfs_src_dir, sdfs_output_dir);
                    }
                }
                break;
            }
            case static_cast<uint32_t>(mj_message::msg_type::job_end): {
                if (auto msg = filter_msg<mj_message::job_end>(response)) {
                    if (msg.value().succeeded) {
                        lg->info("Juice job completed successfully");
                        return true;
                    } else {
                        lg->info("Juice job failed to run successfully");
                        error = "Provided executable failed to run correctly on MapleJuice nodes";
                        return false;
                    }
                }
                break;
            }
            default: {
                lg->info("Received invalid response from Juice master");
                error = "MapleJuice master node sent invalid data";
                return false;
            }
        }
    } while (true);
}

register_auto<juice_client, juice_client_impl> register_juice_client;
