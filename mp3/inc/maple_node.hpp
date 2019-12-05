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
#include "election.h"

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

    struct job_state {
        std::string maple_exe;
        std::string sdfs_intermediate_filename_prefix;
        std::vector<std::string> files;
    };

    // Map from job ID to the state of the job
    std::mutex job_states_mutex;
    std::unordered_map<int, job_state> job_states;

    std::atomic<bool> running;

    // Services that this service depends on
    sdfs_client *sdfsc;
    std::unique_ptr<logger> lg;
    configuration *config;
    std::unique_ptr<tcp_client> client;
    std::unique_ptr<tcp_server> server;
    election *el;
};