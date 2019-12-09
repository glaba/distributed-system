#pragma once

#include "maple_master.h"
#include "environment.h"
#include "logging.h"
#include "configuration.h"
#include "heartbeater.h"
#include "election.h"
#include "tcp.h"
#include "sdfs_master.h"

#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <string>

class maple_master_impl : public maple_master, public service_impl<maple_master_impl> {
public:
    maple_master_impl(environment &env);

    void start();
    void stop();

private:
    void run_master();
    void handle_job(int fd, std::string maple_exe, int num_maples, std::string sdfs_intermediate_filename_prefix, std::string sdfs_src_dir);
    // Assigns files to nodes in the cluster, sends them a message assigning them work, and returns the job ID, which is negative on failure
    int assign_job(std::string maple_exe, int num_maples, std::string sdfs_intermediate_filename_prefix, std::string sdfs_src_dir);
    void assign_job_to_node(int job_id, std::string hostname, std::string maple_exe, std::unordered_set<std::string> input_files, std::string sdfs_intermediate_filename_prefix);
    std::vector<member> get_least_busy_nodes(int n);
    bool job_complete(int job_id);
    void node_dropped(std::string hostname);

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

    struct job_state {
        std::string maple_exe;
        std::string sdfs_intermediate_filename_prefix;

        // A map from node hostname to the files assigned to it that have not yet been processed
        std::unordered_map<std::string, std::unordered_set<std::string>> unprocessed_files;
        std::unordered_map<std::string, std::unordered_set<std::string>> processed_files;

        // A set of (input file, key) pairs that have been committed to the SDFS as the output of Maple
        std::unordered_set<std::pair<std::string, std::string>, string_pair_hash> committed_outputs;
    };

    // RNG to generate job IDs
    std::mt19937 mt;

    // A map from each node in the cluster's hostname to its state, with a mutex protecting it
    std::recursive_mutex node_state_mutex;
    std::unordered_map<std::string, node_state> node_states;

    // A map from job ID to the state of the job, with a mutex protecting it
    std::recursive_mutex job_state_mutex;
    std::unordered_map<int, job_state> job_states;

    // Services that this depends on
    std::unique_ptr<logger> lg;
    configuration *config;
    heartbeater *hb;
    election *el;
    tcp_factory *fac;
    sdfs_master *sdfsm;
    std::unique_ptr<tcp_server> server;

    std::atomic<bool> running;
};
