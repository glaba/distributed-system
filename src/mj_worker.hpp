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
#include "processor.h"
#include "sdfs_master.h"
#include "threadpool.h"
#include "mj_messages.h"
#include "locking.h"

#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdlib.h>
#include <random>
#include <optional>
#include <condition_variable>

class mj_worker_impl : public mj_worker, public service_impl<mj_worker_impl> {
public:
    mj_worker_impl(environment &env);

    void start();
    void stop();

private:
    struct job_state {
        std::string exe;
        std::string sdfs_src_dir;
        std::string sdfs_output_dir;
        processor::type processor_type;
        int num_files_parallel;
        int num_appends_parallel;
        std::unique_ptr<threadpool> tp;

        // Condition variable and indicator for when the job completes (successfully or unsuccessfully)
        mutable std::condition_variable_any cv_done;
        bool job_complete = false;
        bool job_failed = false;
    };
    // Map from job ID to the state of the job
    using job_state_map = std::unordered_map<int, locked<job_state>>;
    locked<job_state_map> job_states_lock;

    // Runs continuously waiting for commands from the master
    void server_thread_function();
    // Runs a Linux command and feeds the results line by line to the callback
    auto run_command(std::string const& command, std::function<bool(std::string const&)> const& callback) const -> bool;
    // Starts a new job, filling in its job_state struct and starting work on the initial set of files
    void start_job(unlocked<job_state_map> &&job_states, int job_id, mj_assign_job const& data);
    // Monitors the progress of a job, informing the master on failure and ending the job when master says it's over
    void monitor_job(int job_id);
    // Notifies the master node that the specified job has failed
    void notify_job_failed(int job_id);
    // Adds the provided files to the queue of files to be processed for the specified job
    void add_files_to_job(int job_id, std::vector<std::string> const& files);
    // Processes a specified file, sending the results into the specified processor, and appending its output into the SDFS
    void process_file(int job_id, std::string const& filename, std::string const& sdfs_src_dir,
        std::string const& sdfs_output_dir, processor::type processor_type, int num_appends_parallel);
    // Appends the output accumulated in the provided processor to files in the SDFS
    void append_output(int job_id, processor *proc, std::string const& input_file,
        std::string const& sdfs_output_dir, int num_appends_parallel);
    // Appends the lines from a single input file to the specified output file after getting permission from the master node
    // Returns either a lambda that will perform the appends or nothing if the master denied permission
    std::optional<std::function<void()>> append_lines(int job_id, tcp_client *client, std::string const& input_file,
        std::string const& output_file_path, std::vector<std::string> const& vals, std::atomic<bool> *master_down);

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
    threadpool_factory *tp_fac;

    std::atomic<bool> running;
    locked<std::mt19937> mt;
};