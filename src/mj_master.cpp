#include "mj_master.h"
#include "mj_master.hpp"
#include "environment.h"
#include "mj_messages.h"
#include "member_list.h"
#include "sdfs.h"
#include "serialization.h"

#include <thread>
#include <chrono>
#include <climits>
#include <functional>
#include <algorithm>

using std::string;
using std::unordered_map;
using std::unordered_set;

mj_master_impl::mj_master_impl(environment &env)
    : mt(std::chrono::system_clock::now().time_since_epoch().count())
    , lg(env.get<logger_factory>()->get_logger("mj_master"))
    , config(env.get<configuration>())
    , hb(env.get<heartbeater>())
    , el(env.get<election>())
    , fac(env.get<tcp_factory>())
    , sdfsm(env.get<sdfs_master>())
    , tp_fac(env.get<threadpool_factory>()), running(false) {}

void mj_master_impl::start() {
    if (running.load()) {
        return;
    }

    lg->info("Starting MapleJuice master");
    el->start();
    sdfsm->start();
    running = true;

    std::thread election_thread([this] {
        bool is_master;
        do {
            el->wait_master_node([&] (member const& m) {
                is_master = (m.id == hb->get_id());
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        } while (!is_master && running.load());
        if (!running.load()) {
            return;
        }

        // Add callbacks for nodes leaving or failing
        std::function<void(member const&)> callback = [this] (member const& m) {
            node_dropped(m.hostname);
        };
        hb->on_leave(callback);
        hb->on_fail(callback);

        // Add callbacks for files being appended to in SDFS so that work can be marked as committed
        sdfsm->on_append({"maplejuice"}, [this] (string const& sdfs_path, unsigned index, sdfs_metadata const& metadata) {
            assert(metadata.find("maplejuice") != metadata.end() && "SDFS master callback logic is incorrect");
            string const& mj_metadata = metadata.find("maplejuice")->second;

            deserializer des(mj_metadata.c_str(), mj_metadata.length());
            string input_file = des.get_string();
            int job_id = des.get_int();

            unlocked<job_state_map> job_states = job_states_lock();
            (*job_states)[job_id].committed_outputs.insert({input_file, sdfs_path});
            lg->trace("Marking " + input_file + ", " + sdfs_path + " as committed");
        });
    });
    election_thread.detach();

    std::thread master_thread([this] {run_master();});
    master_thread.detach();
}

void mj_master_impl::stop() {
    if (!running.load()) {
        return;
    }

    lg->info("Stopping MapleJuice master");
    running = false;
}

void mj_master_impl::run_master() {
    server = fac->get_tcp_server(config->get_mj_master_port());

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

            // Make sure the message is coming from within the group (unless it's a START_JOB message)
            if (hb->get_member_by_id(msg.get_id()).id != msg.get_id() && msg.get_msg_type() != mj_message::mj_msg_type::START_JOB) {
                lg->trace("Received message from member outside of group");
                server->close_connection(fd);
                return;
            }

            bool is_master;
            string master_hostname;
            el->wait_master_node([&] (member const& m) {
                is_master = (m.id == hb->get_id());
                master_hostname = m.hostname;
            });

            if (msg.get_msg_type() == mj_message::mj_msg_type::START_JOB) {
                mj_start_job info = msg.get_msg_data<mj_start_job>();

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

            if (!is_master) {
                server->close_connection(fd);
                return;
            }

            if (msg.get_msg_type() == mj_message::mj_msg_type::JOB_FAILED) {
                mj_job_failed info = msg.get_msg_data<mj_job_failed>();

                lg->info("Received notice from worker node that job with ID " + std::to_string(info.job_id) + " failed, stopping job");

                // Mark the job as failed, which will automatically cause it to complete
                unlocked<job_state_map> job_states = job_states_lock();
                if (job_states->find(info.job_id) != job_states->end()) {
                    (*job_states)[info.job_id].failed = true;
                }
                return;
            }

            while (msg.get_msg_type() == mj_message::mj_msg_type::REQUEST_APPEND_PERM) {
                mj_request_append_perm info = msg.get_msg_data<mj_request_append_perm>();

                bool allow_append;
                {
                    unlocked<job_state_map> job_states = job_states_lock();

                    unordered_set<std::pair<string, string>, string_pair_hash> &committed_outputs =
                        (*job_states)[info.job_id].committed_outputs;
                    allow_append = (committed_outputs.find({info.input_file, info.output_file}) == committed_outputs.end());
                }

                lg->trace("[Job " + std::to_string(info.job_id) + "] Node at " + info.hostname +
                    " requested permission to append values to output file " + info.output_file +
                    " from input file " + info.input_file +
                    (allow_append ? ", allowing append" : ", disallowing append"));

                // Send the permission back to the node
                mj_message perm_msg(hb->get_id(), mj_append_perm{allow_append});
                server->write_to_client(fd, perm_msg.serialize()); // Ignore failure, that will be handled in node_dropped

                // We do not mark the file as committed now, we will mark it when we get a callback from SDFS

                msg_str = server->read_from_client(fd);
                msg = mj_message(msg_str.c_str(), msg_str.length());
                if (!msg.is_well_formed()) {
                    lg->trace("Received ill-formed message from client after it requested permission to append");
                    server->close_connection(fd);
                    return;
                }
                // Check if the client has left the group
                if (hb->get_member_by_id(msg.get_id()).id != msg.get_id()) {
                    lg->trace("Received message from member outside of group");
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
                    unlocked<node_state_map> node_states = node_states_lock();
                    unlocked<job_state_map> job_states = job_states_lock();

                    if (job_states->find(info.job_id) == job_states->end()) {
                        return;
                    }

                    unordered_set<string> &unprocessed_files = (*job_states)[info.job_id].unprocessed_files[info.hostname];
                    unordered_set<string> &processed_files = (*job_states)[info.job_id].processed_files[info.hostname];
                    lg->debug("Node at " + info.hostname + " completed processing file " + info.file +
                        " for job with ID " + std::to_string(info.job_id));
                    assert(unprocessed_files.find(info.file) != unprocessed_files.end() ||
                           processed_files.find(info.file) != processed_files.end() ||
                           !"File that node claims to have completed was not assigned to node");

                    (*node_states)[info.hostname].num_files--;

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

void mj_master_impl::handle_job(int fd, mj_start_job const& info)
{
    // Assign the appropriate nodes a partitioning of the input files based on the specified partitioner
    int job_id = assign_job(info);

    // Wait for the job to actually complete
    while (!job_complete(job_id)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Tell the client that the job is complete
    int succeeded;
    {
        unlocked<job_state_map> job_states = job_states_lock();
        assert(job_states->find(job_id) != job_states->end());
        succeeded = (*job_states)[job_id].failed ? 0 : 1;
    }
    mj_message msg(hb->get_id(), mj_job_end{succeeded});
    server->write_to_client(fd, msg.serialize());
    server->close_connection(fd);

    // Clean up all data and tell the worker nodes that the job is complete
    stop_job(job_id);
}

auto mj_master_impl::job_complete(int job_id) -> bool {
    unlocked<job_state_map> job_states = job_states_lock();

    assert(job_states->find(job_id) != job_states->end());

    // If the job has failed, it has also completed
    if ((*job_states)[job_id].failed) {
        return true;
    }

    // Otherwise, check if there are no more files left in the job
    job_state &state = (*job_states)[job_id];

    unsigned num_files = 0;
    for (auto const& pair : state.unprocessed_files) {
        num_files += pair.second.size();
    }
    lg->info("Waiting on " + std::to_string(num_files) + " files");

    for (auto const& pair : state.unprocessed_files) {
        if (pair.second.size() > 0) {
            return false;
        }
    }
    return true;
}

void mj_master_impl::stop_job(int job_id) {
    // Delete the job information, storing the list of workers
    std::vector<string> workers;
    {
        unlocked<node_state_map> node_states = node_states_lock();
        unlocked<job_state_map> job_states = job_states_lock();
        if (job_states->find(job_id) == job_states->end()) {
            return;
        }

        for (auto const& [worker, _] : (*job_states)[job_id].unprocessed_files) {
            workers.push_back(worker);
        }

        // Delete the job from the node_states, keeping the number of files assigned to the node correct
        for (auto &[worker, state] : *node_states) {
            state.jobs.erase(job_id);
            state.num_files -= (*job_states)[job_id].unprocessed_files[worker].size();
        }
        job_states->erase(job_id);
    }

    // Tell all the nodes in parallel that the job is over
    std::vector<std::thread> stop_threads;
    for (string const& worker : workers) {
        stop_threads.push_back(std::thread([=] {
            mj_message done_msg(hb->get_id(), mj_job_end_worker{job_id});

            std::unique_ptr<tcp_client> client = fac->get_tcp_client(worker, config->get_mj_internal_port());
            if (client.get() == nullptr) {
                return;
            }

            client->write_to_server(done_msg.serialize());
        }));
    }
    for (std::thread &t : stop_threads) {
        t.join();
    }
}

auto mj_master_impl::assign_job(mj_start_job const& info) -> int {
    int job_id = mt() & 0x7FFFFFFF;

    lg->info("Starting new job with parameters [job_id=" + std::to_string(job_id) + ", exe=" + info.exe +
        ", num_workers=" + std::to_string(info.num_workers) + ", partitioner=" + partitioner::print_type(info.partitioner_type) +
        ", sdfs_src_dir=" + info.sdfs_src_dir + ", processor=" + processor::print_type(info.processor_type) +
        ", sdfs_output_dir=" + info.sdfs_output_dir + ", num_files_parallel=" + std::to_string(info.num_files_parallel) +
        ", num_appends_parallel=" + std::to_string(info.num_appends_parallel) + "]");

    // Get the list of input files
    std::optional<std::vector<string>> input_files_opt = sdfsm->ls_files(info.sdfs_src_dir);
    if (!input_files_opt) {
        // TODO: handle failure here
    }

    std::vector<string> input_files = input_files_opt.value();

    // Assign files to nodes based on the specified partitioner and fill the job_state struct
    unordered_map<string, unordered_set<string>> unprocessed_files_copy;
    {
        unlocked<node_state_map> node_states = node_states_lock();
        unlocked<job_state_map> job_states = job_states_lock();

        (*job_states)[job_id].exe = info.exe;
        (*job_states)[job_id].sdfs_src_dir = info.sdfs_src_dir;
        (*job_states)[job_id].sdfs_output_dir = info.sdfs_output_dir;
        (*job_states)[job_id].processor_type = info.processor_type;
        (*job_states)[job_id].num_files_parallel = info.num_files_parallel;
        (*job_states)[job_id].num_appends_parallel = info.num_appends_parallel;
        (*job_states)[job_id].failed = false;

        (*job_states)[job_id].unprocessed_files =
            partitioner_factory::get_partitioner(info.partitioner_type)->partition(hb->get_members(), info.num_workers, input_files);
        unprocessed_files_copy = (*job_states)[job_id].unprocessed_files;

        string members_log_str = "Assigning job with ID " + std::to_string(job_id) + " to: ";
        for (auto const& pair : (*job_states)[job_id].unprocessed_files) {
            (*node_states)[pair.first].num_files += pair.second.size();

            members_log_str += pair.first + " ";
        }
        lg->info(members_log_str);
    }

    { // Print the files assigned to each node
        unlocked<job_state_map> job_states = job_states_lock();
        for (auto const& [hostname, files] : (*job_states)[job_id].unprocessed_files) {
            string log_str = "[Job " + std::to_string(job_id) + "] Files assigned to node at " + hostname + ": ";
            for (auto it = files.begin(); it != files.end(); ++it) {
                log_str += *it;
                if (std::next(it) != files.end()) {
                    log_str += ", ";
                }
            }
            lg->info(log_str);
        }
    }

    // Actually send the message to assign the job to each of the nodes
    std::unique_ptr<threadpool> tp = tp_fac->get_threadpool(unprocessed_files_copy.size());
    for (auto const& [hostname, files] : unprocessed_files_copy) {
        tp->enqueue([=] {assign_job_to_node(job_id, hostname, files);});
    }
    tp->finish();

    return job_id;
}

auto mj_master_impl::get_least_busy_node() -> member {
    std::vector<member> members = hb->get_members();

    // Choose the members that have the least amount of work currently
    unlocked<node_state_map> node_states = node_states_lock();

    unsigned least_busy_index = -1;
    unsigned least_busy_load = UINT_MAX;

    for (unsigned j = 0; j < members.size(); j++) {
        unsigned load = (*node_states)[members[j].hostname].num_files;
        if (load < least_busy_load) {
            least_busy_load = load;
            least_busy_index = j;
        }
    }

    return members[least_busy_index];
}

void mj_master_impl::assign_job_to_node(int job_id, string const& hostname, unordered_set<string> const& input_files)
{
    string exe;
    string sdfs_src_dir;
    string sdfs_output_dir;
    processor::type processor_type;
    int num_files_parallel;
    int num_appends_parallel;
    { // Get the job information
        unlocked<job_state_map> job_states = job_states_lock();
        if (job_states->find(job_id) == job_states->end()) {
            return;
        }
        exe = (*job_states)[job_id].exe;
        sdfs_src_dir = (*job_states)[job_id].sdfs_src_dir;
        sdfs_output_dir = (*job_states)[job_id].sdfs_output_dir;
        processor_type = (*job_states)[job_id].processor_type;
        num_files_parallel = (*job_states)[job_id].num_files_parallel;
        num_appends_parallel = (*job_states)[job_id].num_appends_parallel;
    }

    std::vector<string> input_files_vec(input_files.begin(), input_files.end());
    mj_message msg(hb->get_id(), mj_assign_job{job_id, exe, sdfs_src_dir, input_files_vec,
        processor_type, sdfs_output_dir, num_files_parallel, num_appends_parallel});

    std::unique_ptr<tcp_client> client = fac->get_tcp_client(hostname, config->get_mj_internal_port());
    if (client.get() == nullptr || client->write_to_server(msg.serialize()) <= 0) {
        // The failure will be handled as normal by assigning the files for this node to another node
        lg->trace("Failed to send ASSIGN_JOB message for job with id " + std::to_string(job_id) + " to node at " + hostname);
    } else {
        lg->debug("Sent ASSIGN_JOB message for job with id " + std::to_string(job_id) + " to node at " + hostname);

        // Mark the job as assigned to this node
        unlocked<node_state_map> node_states = node_states_lock();
        (*node_states)[hostname].jobs.insert(job_id);
    }
}

// Find all the files that this node was responsible for that it did not yet process and assign them
// TODO: make this process resilient so that if we crash during it, the new master can pick up at the right place
void mj_master_impl::node_dropped(string const& hostname) {

    lg->info("Lost node at " + hostname + ", reassigning its work to other nodes");

    // Map from job ID to <hostname, files newly assigned to hostname>
    unordered_map<int, std::tuple<string, unordered_set<string>>> assignments;
    {
        unlocked<node_state_map> node_states = node_states_lock();
        unlocked<job_state_map> job_states = job_states_lock();

        // Save which jobs it was a part of then delete all information associated with the node
        unordered_set<int> jobs = (*node_states)[hostname].jobs;
        node_states->erase(hostname);

        for (int job_id : jobs) {
            // Redistribute the work for this job to the least busy node
            string target = get_least_busy_node().hostname;
            assignments[job_id] = {target, (*job_states)[job_id].unprocessed_files[hostname]};

            // Erase all data associated with the dropped node for this job
            (*job_states)[job_id].unprocessed_files.erase(hostname);
            (*job_states)[job_id].processed_files.erase(hostname);

            lg->info("Redistributing work of node " + hostname + " on job with ID " + std::to_string(job_id) + " to node " + target);

            // Mark the newly assigned files as belonging to the target node
            auto const& [_, files] = assignments[job_id];
            for (string const& file : files) {
                (*job_states)[job_id].unprocessed_files[target].insert(file);
            }
            (*node_states)[target].num_files += files.size();
        }
    }

    std::thread assign_thread([=] () mutable {
        // Wait for any transactions that are running to complete so that files this node was in the middle of writing get written
        sdfsm->wait_transactions();

        // Send the message to assign the new work to each of the nodes
        for (auto const& [job_id, assignment] : assignments) {
            auto const& [target, files] = assignment;
            assign_job_to_node(job_id, target, files);
        }
    });
    assign_thread.detach();
}

register_auto<mj_master, mj_master_impl> register_mj_master;
