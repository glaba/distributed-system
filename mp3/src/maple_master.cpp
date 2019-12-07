#include "maple_master.h"
#include "maple_master.hpp"
#include "environment.h"
#include "maple_messages.h"
#include "member_list.h"

#include <thread>
#include <chrono>
#include <climits>
#include <functional>

using std::string;

maple_master_impl::maple_master_impl(environment &env)
    : lg(env.get<logger_factory>()->get_logger("maple_master"))
    , config(env.get<configuration>())
    , hb(env.get<heartbeater>())
    , el(env.get<election>())
    , client(env.get<tcp_factory>()->get_tcp_client())
    , server(env.get<tcp_factory>()->get_tcp_server()) {}

void maple_master_impl::start() {
    lg->info("Starting Maple master");
    el->start();
    running = true;

    // Add callbacks for nodes leaving or failing
    std::function<void(member)> callback = [this] (member m) {
        node_dropped(m.hostname);
    };
    hb->on_leave(callback);
    hb->on_fail(callback);

    std::thread master_thread([this] {run_master();});
    master_thread.detach();
}

void maple_master_impl::stop() {
    lg->info("Stopping Maple master");
    running = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

void maple_master_impl::run_master() {
    server->setup_server(config->get_maple_master_port());

    while (true) {
        if (!running.load()) {
            break;
        }

        int fd = server->accept_connection();
        if (fd < 0) {
            continue;
        }

        std::thread client_thread([this, fd] {
            string msg_str = server->read_from_client(fd);
            maple_message msg(msg_str.c_str(), msg_str.length());

            if (!msg.is_well_formed()) {
                lg->debug("Received ill-formed message from client");
                return;
            }

            if (msg.get_msg_type() == maple_message::maple_msg_type::START_JOB) {
                maple_start_job info = msg.get_msg_data<maple_start_job>();

                bool is_master;
                string master_hostname;
                el->get_master_node([this, &is_master, &master_hostname] (member m, bool succeeded) {
                    is_master = (m.id == hb->get_id());
                    master_hostname = m.hostname;
                });

                if (is_master) {
                    handle_job(fd, info.maple_exe, info.num_maples, info.sdfs_intermediate_filename_prefix, info.sdfs_src_dir);
                } else {
                    lg->trace("Redirecting request to start job with executable " + info.maple_exe + " to master node");

                    // Send a message to the client informing them of the actual master node
                    maple_message not_master_msg(maple_not_master{master_hostname});
                    server->write_to_client(fd, not_master_msg.serialize());
                    server->close_connection(fd);
                }
            }

            if (msg.get_msg_type() == maple_message::maple_msg_type::REQUEST_APPEND_PERM) {
                maple_request_append_perm info = msg.get_msg_data<maple_request_append_perm>();

                bool allow_append;
                {
                    std::lock_guard<std::recursive_mutex> guard_job_states(job_state_mutex);

                    std::unordered_set<std::pair<string, string>, string_pair_hash> &committed_outputs =
                        job_states[info.job_id].committed_outputs;
                    allow_append = (committed_outputs.find(std::make_pair(info.file, info.key)) == committed_outputs.end());
                }

                lg->debug("Node at " + info.hostname + " requested permission to append values for key " + info.key +
                    " from input file " + info.file + " for job with ID " + std::to_string(info.job_id));

                // Send the permission back to the node
                maple_message msg(maple_append_perm{allow_append});
                server->write_to_client(fd, msg.serialize()); // Ignore failure, that will be handled in node_dropped
                server->close_connection(fd);
            }

            if (msg.get_msg_type() == maple_message::maple_msg_type::FILE_DONE) {
                maple_file_done info = msg.get_msg_data<maple_file_done>();

                {
                    std::lock_guard<std::recursive_mutex> guard_node_states(node_state_mutex);
                    std::lock_guard<std::recursive_mutex> guard_job_states(job_state_mutex);

                    std::unordered_set<std::string> &unprocessed_files = job_states[info.job_id].unprocessed_files[info.hostname];
                    std::unordered_set<std::string> &processed_files = job_states[info.job_id].processed_files[info.hostname];
                    lg->info("Node at " + info.hostname + " completed processing file " + info.file +
                        " for job with ID " + std::to_string(info.job_id));
                    assert(unprocessed_files.find(info.file) != unprocessed_files.end() ||
                           processed_files.find(info.file) != processed_files.end() ||
                           !"File that node claims to have completed was not assigned to node");

                    node_states[info.hostname].num_files--; // Even if the node fails, this is OK because this will be erased

                    if (processed_files.find(info.file) != processed_files.end()) {
                        unprocessed_files.erase(info.file);
                        processed_files.insert(info.file);
                    }
                }

                server->close_connection(fd);
            }
        });
        client_thread.detach();
    }
}

void maple_master_impl::handle_job(int fd, string maple_exe, int num_maples,
    string sdfs_intermediate_filename_prefix, string sdfs_src_dir)
{
    // Assign the appropriate nodes an even partitioning of the input files
    assign_job(maple_exe, num_maples, sdfs_intermediate_filename_prefix, sdfs_src_dir);

    // Tell the client that the job is complete
    maple_message msg(maple_job_end{1});
    server->write_to_client(fd, msg.serialize());
    server->close_connection(fd);
}

int maple_master_impl::assign_job(std::string maple_exe, int num_maples,
    std::string sdfs_intermediate_filename_prefix, std::string sdfs_src_dir)
{
    int job_id = mt() & 0x7FFFFFFF;

    lg->info("Starting new job with parameters [job_id=" + std::to_string(job_id) + ", maple_exe=" + maple_exe +
        ", num_maples=" + std::to_string(num_maples) + ", sdfs_intermediate_filename_prefix=" +
        sdfs_intermediate_filename_prefix + ", sdfs_src_dir=" + sdfs_src_dir + "]");

    // TODO: get the list of input files
    std::vector<std::string> input_files;

    std::vector<member> members = get_least_busy_nodes(num_maples);

    string members_log_str = "Assigning job with ID " + std::to_string(job_id) + " to: ";
    for (unsigned i = 0; i < members.size(); i++) {
        members_log_str += members[i].hostname;
        if (i < members.size() - 1) {
            members_log_str += ", ";
        }
    }
    lg->info(members_log_str);

    { // Add files in a round robin fashion
        std::lock_guard<std::recursive_mutex> guard_node_states(node_state_mutex);
        std::lock_guard<std::recursive_mutex> guard_job_states(job_state_mutex);

        for (unsigned i = 0; i < input_files.size(); i++) {
            unsigned member_index = i % members.size();
            string hostname = members[member_index].hostname;
            node_states[hostname].num_files++;
            job_states[job_id].unprocessed_files[hostname].insert(input_files[i]);
        }
    }

    // Actually send the message to assign the job to each of the nodes
    for (unsigned i = 0; i < members.size(); i++) {
        assign_job_to_node(job_id, members[i].hostname, maple_exe, job_states[job_id].unprocessed_files[members[i].hostname],
            sdfs_intermediate_filename_prefix);
    }

    return job_id;
}

std::vector<member> maple_master_impl::get_least_busy_nodes(int n) {
    std::vector<member> all_members = hb->get_members();
    std::vector<member> members;
    // Choose the members that have the least amount of work currently
    {
        std::lock_guard<std::recursive_mutex> guard_node_states(node_state_mutex);

        for (int i = 0; i < n; i++) {
            unsigned least_busy_index = -1;
            unsigned least_busy_load = UINT_MAX;

            if (all_members.size() == 0) {
                break;
            }

            for (unsigned j = 0; j < all_members.size(); j++) {
                unsigned load = node_states[all_members[j].hostname].num_files;
                if (load < least_busy_load) {
                    least_busy_load = load;
                    least_busy_index = j;
                }
            }

            members.push_back(all_members[least_busy_index]);
            all_members.erase(all_members.begin() + least_busy_index);
        }
    }
    return members;
}

void maple_master_impl::assign_job_to_node(int job_id, std::string hostname, std::string maple_exe,
    std::unordered_set<std::string> input_files, std::string sdfs_intermediate_filename_prefix)
{
    std::vector<string> input_files_vec(input_files.begin(), input_files.end());
    maple_message msg(maple_assign_job{job_id, maple_exe, input_files_vec, sdfs_intermediate_filename_prefix});

    int fd = client->setup_connection(hostname, config->get_maple_internal_port());
    if (fd < 0 || client->write_to_server(fd, msg.serialize()) <= 0) {
        // The failure will be handled as normal by assigning the files for this node to another node
        lg->trace("Failed to send ASSIGN_JOB message for job with id " + std::to_string(job_id) + " to node at " + hostname);
        client->close_connection(fd);
    } else {
        lg->debug("Sent ASSIGN_JOB message for job with id " + std::to_string(job_id) + " to node at " + hostname);
        client->close_connection(fd);

        // Mark the job as assigned to this node
        node_states[hostname].jobs.insert(job_id);
    }
}

// Find all the files that this node was responsible for that it did not yet process and assign them
// TODO: make this process resilient so that if we crash during it, the new master can pick up at the right place
void maple_master_impl::node_dropped(std::string hostname) {
    std::unordered_set<int> jobs;

    lg->info("Lost node at " + hostname + ", reassigning its work to other nodes");

    { // Get the jobs it was a part of and then delete it
        std::lock_guard<std::recursive_mutex> guard_node_states(node_state_mutex);
        jobs = node_states[hostname].jobs;
        node_states.erase(hostname);
    }

    std::unordered_map<int, job_state> participating_job_states;
    for (int job_id : jobs) {
        { // Get the job states that this node was a part of and then erase its portion
            std::lock_guard<std::recursive_mutex> guard_job_states(job_state_mutex);
            participating_job_states[job_id] = job_states[job_id];
            job_states[job_id].unprocessed_files.erase(hostname);
            job_states[job_id].processed_files.erase(hostname);
        }
    }

    // Redistribute the work for each job to the least busy node
    std::unordered_map<int, string> targets;
    {
        std::lock_guard<std::recursive_mutex> guard_node_states(node_state_mutex);
        std::lock_guard<std::recursive_mutex> guard_job_states(job_state_mutex);
        for (int job_id : jobs) {
            string target = get_least_busy_nodes(1)[0].hostname;
            targets[job_id] = target;

            lg->debug("Redistributing work of node " + hostname + " on job with ID " + std::to_string(job_id) + " to node " + target);

            job_states[job_id].unprocessed_files[target] = participating_job_states[job_id].unprocessed_files[hostname];
            node_states[target].num_files += participating_job_states[job_id].unprocessed_files[hostname].size();
        }
    }

    // Send the message to assign the new work to each of the nodes
    for (int job_id : jobs) {
        assign_job_to_node(job_id, targets[job_id], participating_job_states[job_id].maple_exe,
            participating_job_states[job_id].unprocessed_files[hostname],
            participating_job_states[job_id].sdfs_intermediate_filename_prefix);
    }
}

register_auto<maple_master, maple_master_impl> register_maple_master;
