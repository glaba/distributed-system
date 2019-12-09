#pragma once

#include "maple_node.h"
#include "environment.h"
#include "logging.h"
#include "sdfs_client.h"
#include "configuration.h"
#include "tcp.h"
#include "logging.h"
#include "configuration.h"
#include "sdfs_client.h"
#include "sdfs_server.h"
#include "election.h"
#include "maple_master.h"
#include "heartbeater.h"

#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <functional>

class maple_node_impl : public maple_node, public service_impl<maple_node_impl> {
public:
    maple_node_impl(environment &env);

    void start();
    void stop();

private:
    void server_thread_function();
    bool run_command(std::string command, std::function<bool(std::string)> callback);
    void run_job(int job_id);
    bool append_kv_pairs(int job_id, std::unordered_map<std::string, std::vector<std::string>> &kv_pairs,
        std::string sdfs_intermediate_filename_prefix, std::string filename);

    bool retry(std::function<bool()> callback, int job_id, std::string description);

    struct job_state {
        std::string maple_exe;
        std::string sdfs_intermediate_filename_prefix;
        std::vector<std::string> files;
    };

    // Map from job ID to the state of the job
    std::mutex job_states_mutex;
    std::unordered_map<int, job_state> job_states;

    // Services that this service depends on
    std::unique_ptr<logger> lg;
    configuration *config;
    tcp_factory *fac;
    std::unique_ptr<tcp_server> server;
    heartbeater *hb;
    election *el;
    maple_master *mm;
    sdfs_client *sdfsc;
    sdfs_server *sdfss;

    std::atomic<bool> running;
};