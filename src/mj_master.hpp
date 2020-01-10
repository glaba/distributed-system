#pragma once

#include "mj_messages.h"
#include "mj_master.h"
#include "environment.h"
#include "logging.h"
#include "configuration.h"
#include "heartbeater.h"
#include "election.h"
#include "tcp.h"
#include "sdfs_master.h"
#include "processor.h"
#include "threadpool.h"
#include "locking.h"

#include <memory>
#include <atomic>
#include <unordered_map>
#include <string>

class mj_master_impl : public mj_master, public service_impl<mj_master_impl> {
public:
    mj_master_impl(environment &env);

    void start();
    void stop();

private:
    // Server thread which listens for incoming TCP messages as a master node
    void run_master();
    // Initiates a new job, waits for it to complete, and informs the client that started the job
    void handle_job(int fd, mj_start_job const& info);
    // Assigns files to nodes in the cluster, sends them a message assigning them work,
    // and returns the job ID, which is negative on failure
    auto assign_job(mj_start_job const& info) -> int;
    // Specifically assigns a list of input files to the provided node
    void assign_job_to_node(int job_id, std::string const& hostname, std::unordered_set<std::string> const& input_files);
    // Returns the node with the current least amount of files being processed
    auto get_least_busy_node() -> member;
    // Notifies all worker nodes to stop working on the specified job, and cleans up any related data
    void stop_job(int job_id);
    // Checks whether or not a previously running job has completed
    auto job_complete(int job_id) -> bool;
    // Callback called by heartbeater when a node goes down, which will redistribute its work to other nodes
    void node_dropped(std::string const& hostname);

    struct string_pair_hash {
        inline std::size_t operator()(const std::pair<std::string, std::string> &v) const {
            return (static_cast<size_t>(static_cast<uint32_t>(std::hash<std::string>()(v.first))) << 32) +
                   static_cast<size_t>(static_cast<uint32_t>(std::hash<std::string>()(v.second)));
        }
    };

    struct node_state {
        unsigned num_files; // The number of files being processed by this node
        std::unordered_set<int> jobs;
    };
    // A map from each node in the cluster's hostname to its state
    using node_state_map = std::unordered_map<std::string, node_state>;
    locked<node_state_map> node_states_lock;

    struct job_state {
        std::string exe;
        std::string sdfs_src_dir;
        std::string sdfs_output_dir;
        processor::type processor_type;
        int num_files_parallel;
        int num_appends_parallel;

        // A map from node hostname to the files assigned to it that have not yet been processed
        std::unordered_map<std::string, std::unordered_set<std::string>> unprocessed_files;
        std::unordered_map<std::string, std::unordered_set<std::string>> processed_files;

        // A set of (input file, output file) pairs that have been committed to the SDFS
        // TODO: this is actually not sufficient, since there might be multiple outputs from a given input file
        std::unordered_set<std::pair<std::string, std::string>, string_pair_hash> committed_outputs;

        bool failed;
    };
    // A map from job ID to the state of the job
    using job_state_map = std::unordered_map<int, job_state>;
    locked<job_state_map> job_states_lock;

    // RNG to generate job IDs
    std::mt19937 mt;

    // Services that this depends on
    std::unique_ptr<logger> lg;
    configuration *config;
    heartbeater *hb;
    election *el;
    tcp_factory *fac;
    sdfs_master *sdfsm;
    std::unique_ptr<tcp_server> server;
    threadpool_factory *tp_fac;

    std::atomic<bool> running;
};
