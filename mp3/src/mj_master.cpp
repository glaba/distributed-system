#include "mj_master.h"
#include "mj_master.hpp"
#include "environment.h"
#include "mj_messages.h"
#include "member_list.h"

#include <thread>
#include <chrono>
#include <climits>
#include <functional>
#include <algorithm>

using std::string;

mj_master_impl::mj_master_impl(environment &env)
    : mt(std::chrono::system_clock::now().time_since_epoch().count())
    , lg(env.get<logger_factory>()->get_logger("mj_master"))
    , config(env.get<configuration>())
    , hb(env.get<heartbeater>())
    , el(env.get<election>())
    , fac(env.get<tcp_factory>())
    , sdfsm(env.get<sdfs_master>())
    , server(env.get<tcp_factory>()->get_tcp_server()), running(false) {}

void mj_master_impl::start() {
    if (running.load()) {
        return;
    }

    lg->info("Starting MapleJuice master");
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

void mj_master_impl::stop() {
    if (!running.load()) {
        return;
    }

    lg->info("Stopping MapleJuice master");
    running = false;
    server->stop_server();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

void mj_master_impl::run_master() {
    server->setup_server(config->get_mj_master_port());

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
            mj_message msg(msg_str.c_str(), msg_str.length());

            if (!msg.is_well_formed()) {
                lg->debug("Received ill-formed message from client");
                server->close_connection(fd);
                return;
            }

            // Make sure the message is coming from within the group
            if (hb->get_member_by_id(msg.get_id()).id != msg.get_id()) {
                lg->info("Received message from member outside of group");
                server->close_connection(fd);
                return;
            }

            if (msg.get_msg_type() == mj_message::mj_msg_type::START_JOB) {
                mj_start_job info = msg.get_msg_data<mj_start_job>();

                bool is_master;
                string master_hostname;
                el->wait_master_node([this, &is_master, &master_hostname] (member m) {
                    is_master = (m.id == hb->get_id());
                    master_hostname = m.hostname;
                });

                if (is_master) {
                    handle_job(fd, info);
                } else {
                    lg->trace("Redirecting request to start job with executable " + info.exe + " to master node");

                    // Send a message to the client informing them of the actual master node
                    mj_message not_master_msg(hb->get_id(), mj_not_master{master_hostname});
                    server->write_to_client(fd, not_master_msg.serialize());
                    server->close_connection(fd);
                }
                return;
            }

            while (msg.get_msg_type() == mj_message::mj_msg_type::REQUEST_APPEND_PERM) {
                mj_request_append_perm info = msg.get_msg_data<mj_request_append_perm>();

                bool allow_append;
                {
                    std::lock_guard<std::recursive_mutex> guard_job_states(job_state_mutex);

                    std::unordered_set<std::pair<string, string>, string_pair_hash> &committed_outputs =
                        job_states[info.job_id].committed_outputs;
                    allow_append = (committed_outputs.find(std::make_pair(info.input_file, info.output_file)) == committed_outputs.end());
                }

                lg->debug("[Job " + std::to_string(info.job_id) + "] Node at " + info.hostname +
                    " requested permission to append values to output file " + info.output_file +
                    " from input file " + info.input_file +
                    (allow_append ? ", allowing append" : ", disallowing append"));

                // Send the permission back to the node
                mj_message perm_msg(hb->get_id(), mj_append_perm{allow_append});
                server->write_to_client(fd, perm_msg.serialize()); // Ignore failure, that will be handled in node_dropped

                // TODO: use callback from SDFS to reliably mark the output as committed instead of this
                {
                    std::lock_guard<std::recursive_mutex> guard_job_states(job_state_mutex);
                    job_states[info.job_id].committed_outputs.insert(std::make_pair(info.input_file, info.output_file));
                }

                msg_str = server->read_from_client(fd);
                msg = mj_message(msg_str.c_str(), msg_str.length());
                if (!msg.is_well_formed()) {
                    lg->debug("Received ill-formed message from client after it requested permission to append");
                    server->close_connection(fd);
                    return;
                }
                // Check if the client has left the group
                if (hb->get_member_by_id(msg.get_id()).id != msg.get_id()) {
                    lg->info("Received message from member outside of group");
                    server->close_connection(fd);
                    return;
                }
            }

            // Either we arrived here from a new connection OR after a REQUEST_APPEND_PERM from the previous if block
            // Either way, we should close the connection with the server
            server->close_connection(fd);
            if (msg.get_msg_type() == mj_message::mj_msg_type::FILE_DONE) {
                mj_file_done info = msg.get_msg_data<mj_file_done>();

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

                    if (processed_files.find(info.file) == processed_files.end()) {
                        unprocessed_files.erase(info.file);
                        processed_files.insert(info.file);
                    }
                }
                return;
            }

            lg->debug("Received unexpected message type from client");
        });
        client_thread.detach();
    }
}

void mj_master_impl::handle_job(int fd, mj_start_job info)
{
    // Assign the appropriate nodes a partitioning of the input files based on the specified partitioner
    int job_id = assign_job(info);

    // Wait for the job to actually complete
    while (!job_complete(job_id)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Tell the client that the job is complete
    mj_message msg(hb->get_id(), mj_job_end{1});
    server->write_to_client(fd, msg.serialize());
    server->close_connection(fd);
}

bool mj_master_impl::job_complete(int job_id) {
    std::lock_guard<std::recursive_mutex> guard(job_state_mutex);

    job_state &state = job_states[job_id];
    for (auto pair : state.unprocessed_files) {
        if (pair.second.size() > 0) {
            return false;
        }
    }
    return true;
}

int mj_master_impl::assign_job(mj_start_job info) {
    int job_id = mt() & 0x7FFFFFFF;

    lg->info("Starting new job with parameters [job_id=" + std::to_string(job_id) + ", exe=" + info.exe +
        ", num_workers=" + std::to_string(info.num_workers) + "partitioner=" + partitioner::print_type(info.partitioner_type) +
        ", sdfs_src_dir=" + info.sdfs_src_dir + ", outputter=" + outputter::print_type(info.outputter_type) +
        ", sdfs_output_dir=" + info.sdfs_output_dir + "]");

    // Get the list of input files
    std::vector<std::string> input_files_raw = sdfsm->get_files_by_prefix(info.sdfs_src_dir);
    std::unordered_set<std::string> input_files_set;
    std::vector<std::string> input_files;
    // Strip the directory from the input files as well as shard numbers
    for (std::string &file : input_files_raw) {
        file = file.substr(info.sdfs_src_dir.length());
        file = file.substr(0, file.find_last_of("."));
        input_files_set.insert(file);
        lg->info("Sanitized: " + file);
    }
    for (const std::string &file : input_files_set) {
        input_files.push_back(file);
        lg->info(file);
    }

    // Assign files to nodes based on the specified partitioner and fill the job_state struct
    std::unordered_map<std::string, std::unordered_set<std::string>> unprocessed_files_copy;
    {
        std::lock_guard<std::recursive_mutex> guard_node_states(node_state_mutex);
        std::lock_guard<std::recursive_mutex> guard_job_states(job_state_mutex);

        job_states[job_id].exe = info.exe;
        job_states[job_id].sdfs_src_dir = info.sdfs_src_dir;
        job_states[job_id].sdfs_output_dir = info.sdfs_output_dir;
        job_states[job_id].outputter_type = info.outputter_type;

        job_states[job_id].unprocessed_files =
            partitioner_factory::get_partitioner(info.partitioner_type)->partition(hb->get_members(), info.num_workers, input_files);
        unprocessed_files_copy = job_states[job_id].unprocessed_files;

        string members_log_str = "Assigning job with ID " + std::to_string(job_id) + " to: ";
        for (auto &pair : job_states[job_id].unprocessed_files) {
            node_states[pair.first].num_files += pair.second.size();

            members_log_str += pair.first + " ";
        }
        lg->info(members_log_str);
    }

    { // Print the files assigned to each node
        std::lock_guard<std::recursive_mutex> guard(job_state_mutex);
        for (auto &pair : job_states[job_id].unprocessed_files) {
            string log_str = "[Job " + std::to_string(job_id) + "] Files assigned to node at " + pair.first + ": ";
            for (auto it = pair.second.begin(); it != pair.second.end(); ++it) {
                log_str += *it;
                if (std::next(it) != pair.second.end()) {
                    log_str += ", ";
                }
            }
            lg->info(log_str);
        }
    }

    // Actually send the message to assign the job to each of the nodes
    for (auto &pair : unprocessed_files_copy) {
        assign_job_to_node(job_id, pair.first, pair.second);
    }

    return job_id;
}

member mj_master_impl::get_least_busy_node() {
    std::vector<member> members = hb->get_members();
    // Choose the members that have the least amount of work currently
    {
        std::lock_guard<std::recursive_mutex> guard_node_states(node_state_mutex);

        unsigned least_busy_index = -1;
        unsigned least_busy_load = UINT_MAX;

        for (unsigned j = 0; j < members.size(); j++) {
            unsigned load = node_states[members[j].hostname].num_files;
            if (load < least_busy_load) {
                least_busy_load = load;
                least_busy_index = j;
            }
        }

        return members[least_busy_index];
    }
}

void mj_master_impl::assign_job_to_node(int job_id, std::string hostname, std::unordered_set<std::string> input_files)
{
    std::string exe;
    std::string sdfs_src_dir;
    std::string sdfs_output_dir;
    outputter::type outputter_type;
    {
        std::lock_guard<std::recursive_mutex> guard(job_state_mutex);
        exe = job_states[job_id].exe;
        sdfs_src_dir = job_states[job_id].sdfs_src_dir;
        sdfs_output_dir = job_states[job_id].sdfs_output_dir;
        outputter_type = job_states[job_id].outputter_type;
    }

    std::vector<string> input_files_vec(input_files.begin(), input_files.end());
    mj_message msg(hb->get_id(), mj_assign_job{job_id, exe, sdfs_src_dir, input_files_vec, outputter_type, sdfs_output_dir});

    std::unique_ptr<tcp_client> client = fac->get_tcp_client();
    int fd = client->setup_connection(hostname, config->get_mj_internal_port());
    if (fd < 0 || client->write_to_server(fd, msg.serialize()) <= 0) {
        // The failure will be handled as normal by assigning the files for this node to another node
        lg->trace("Failed to send ASSIGN_JOB message for job with id " + std::to_string(job_id) + " to node at " + hostname);
        client->close_connection(fd);
    } else {
        lg->debug("Sent ASSIGN_JOB message for job with id " + std::to_string(job_id) + " to node at " + hostname);
        client->close_connection(fd);

        // Mark the job as assigned to this node
        std::lock_guard<std::recursive_mutex> guard_node_states(node_state_mutex);
        node_states[hostname].jobs.insert(job_id);
    }
}

// Find all the files that this node was responsible for that it did not yet process and assign them
// TODO: make this process resilient so that if we crash during it, the new master can pick up at the right place
void mj_master_impl::node_dropped(std::string hostname) {
    bool is_master = false;
    el->get_master_node([this, &is_master] (member m, bool succeeded) {
        if (succeeded && m.id == hb->get_id()) {
            is_master = true;
        }
    });
    if (!is_master) {
        return;
    }

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
            string target = get_least_busy_node().hostname;
            targets[job_id] = target;

            lg->info("Redistributing work of node " + hostname + " on job with ID " + std::to_string(job_id) + " to node " + target);

            for (string file : participating_job_states[job_id].unprocessed_files[hostname]) {
                job_states[job_id].unprocessed_files[target].insert(file);
            }
            node_states[target].num_files += participating_job_states[job_id].unprocessed_files[hostname].size();
        }
    }

    // Send the message to assign the new work to each of the nodes
    for (int job_id : jobs) {
        assign_job_to_node(job_id, targets[job_id], participating_job_states[job_id].unprocessed_files[hostname]);
    }
}

register_auto<mj_master, mj_master_impl> register_mj_master;
