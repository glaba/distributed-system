#pragma once

#include "mj_worker.h"
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
#include "mj_master.h"
#include "heartbeater.h"
#include "outputter.h"
#include "sdfs_master.h"

#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdlib.h>
#include <random>

class mj_worker_impl : public mj_worker, public service_impl<mj_worker_impl> {
public:
    mj_worker_impl(environment &env);

    void start();
    void stop();

private:
    void server_thread_function();
    bool run_command(std::string command, std::function<bool(std::string)> callback);
    void run_job(int job_id);
    bool append_output(int job_id, outputter *outptr, std::string input_file);

    bool retry(std::function<bool()> callback, int job_id, std::string description);

    struct job_state {
        std::string exe;
        std::string sdfs_src_dir;
        std::string sdfs_output_dir;
        outputter::type outputter_type;
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
    mj_master *mm;
    sdfs_client *sdfsc;
    sdfs_server *sdfss;
    sdfs_master *sdfsm;

    std::atomic<bool> running;
    std::mt19937 mt;
};