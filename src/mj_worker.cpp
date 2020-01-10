#include "mj_worker.h"
#include "mj_worker.hpp"
#include "environment.h"
#include "mj_messages.h"
#include "serialization.h"
#include "member_list.h"
#include "threadpool.h"
#include "utils.h"

#include <stdlib.h>
#include <algorithm>
#include <fstream>

using std::string;
using std::optional;
using std::function;

mj_worker_impl::mj_worker_impl(environment &env)
    : lg(env.get<logger_factory>()->get_logger("mj_worker"))
    , config(env.get<configuration>())
    , fac(env.get<tcp_factory>())
    , hb(env.get<heartbeater>())
    , el(env.get<election>())
    , mm(env.get<mj_master>())
    , sdfsc(env.get<sdfs_client>())
    , sdfss(env.get<sdfs_server>())
    , sdfsm(env.get<sdfs_master>())
    , tp_fac(env.get<threadpool_factory>()), running(false), mt(std::chrono::system_clock::now().time_since_epoch().count()) {}

void mj_worker_impl::start() {
    if (running.load()) {
        return;
    }

    lg->info("Starting MapleJuice node");
    el->start();
    mm->start();
    sdfsc->start();
    sdfss->start();
    sdfsm->start();
    running = true;

    std::thread server_thread([this] {server_thread_function();});
    server_thread.detach();
}

void mj_worker_impl::stop() {
    if (!running.load()) {
        return;
    }

    lg->info("Stopping MapleJuice worker");

    {
        unlocked<job_state_map> job_states = job_states_lock();
        std::vector<unlocked<job_state>> states;

        // Lock each job before setting running to false
        for (auto const& [job_id, state_lock] : *job_states) {
            states.push_back(state_lock());
        }

        running = false;

        // Notify each job that we are no longer running
        for (auto const& state : states) {
            state->cv_done.notify_all();
        }
    }

    sdfsc->stop();
    sdfss->stop();
    sdfsm->stop();
    mm->stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    el->stop();
}

void mj_worker_impl::server_thread_function() {
    server = fac->get_tcp_server(config->get_mj_internal_port());

    while (true) {
        // Wait for the master node to connect to us and assign us work
        int fd = server->accept_connection();

        if (!running.load()) {
            lg->debug("Exiting server");
            return;
        }

        if (fd < 0) {
            continue;
        }

        std::thread client_thread([this, fd] {
            // Get the message from the master and parse it
            string msg_str = server->read_from_client(fd);
            server->close_connection(fd);

            mj_message msg(msg_str.c_str(), msg_str.length());

            // We only handle messages of the type ASSIGN_JOB or JOB_END_WORKER
            if (!msg.is_well_formed() || (msg.get_msg_type() != mj_message::mj_msg_type::ASSIGN_JOB &&
                msg.get_msg_type() != mj_message::mj_msg_type::JOB_END_WORKER))
            {
                lg->debug("Received malformed message from master, meaning master has most likely crashed");
                return;
            }

            if (msg.get_msg_type() == mj_message::mj_msg_type::ASSIGN_JOB) {
                mj_assign_job data = msg.get_msg_data<mj_assign_job>();
                int job_id = data.job_id;

                bool already_running;
                { // Start an atomic block to access the job_states map
                    unlocked<job_state_map> job_states = job_states_lock();
                    already_running = (job_states->find(job_id) != job_states->end());

                    // Spin up a thread to process the job if it isn't already running
                    if (!already_running) {
                        start_job(std::move(job_states), job_id, data);
                        std::thread monitor_thread([=] {
                            monitor_job(job_id);
                        });
                        monitor_thread.detach();
                        lg->info("Starting new job with ID " + std::to_string(job_id));
                    } else {
                        std::thread job_thread([=] {
                            add_files_to_job(job_id, data.input_files);
                        });
                        job_thread.detach();
                        lg->info("Accepting extra work due to loss of a node for job with ID " + std::to_string(job_id));
                    }
                }
            }

            if (msg.get_msg_type() == mj_message::mj_msg_type::JOB_END_WORKER) {
                mj_job_end_worker data = msg.get_msg_data<mj_job_end_worker>();

                { // Atomic block to access job_states map
                    unlocked<job_state_map> job_states = job_states_lock();

                    int job_id = data.job_id;
                    if (job_states->find(job_id) != job_states->end()) {
                        unlocked<job_state> state = (*job_states)[job_id]();

                        // Notify the condition variable that the job is over, which monitor_job is waiting on
                        state->job_complete = true;
                        state->cv_done.notify_all();
                        lg->info("Master node has informed us that job with ID " + std::to_string(job_id) + " is done");
                    }
                }
            }
        });
        client_thread.detach();
    }
}

void mj_worker_impl::start_job(unlocked<job_state_map> &&job_states, int job_id, mj_assign_job const& data) {
    // Create an entry in job_states and then unlock it
    unlocked<job_state> state = (*job_states)[job_id]();

    string exe_path = config->get_mj_dir() + "exe_" + std::to_string(job_id);

    state->exe = data.exe;
    state->sdfs_src_dir = data.sdfs_src_dir;
    state->sdfs_output_dir = data.sdfs_output_dir;
    state->processor_type = data.processor_type;
    state->num_files_parallel = data.num_files_parallel;
    state->num_appends_parallel = data.num_appends_parallel;
    state->tp = tp_fac->get_threadpool(data.num_files_parallel);

    // Download exe from the SDFS
    utils::backoff([&] {
        // TODO: handle the executable being missing, as opposed to a failed get
        return sdfsc->get(exe_path, state->exe) == 0;
    });
    FILE *stream = popen(("chmod +x \"" + exe_path + "\"").c_str(), "r");
    pclose(stream);
    lg->debug("[Job " + std::to_string(job_id) + "] Downloaded executable " + state->exe + " from SDFS");

    std::thread add_files_thread([=] {
        add_files_to_job(job_id, data.input_files);
    });
    add_files_thread.detach();
}

void mj_worker_impl::monitor_job(int job_id) {
    // Wait on the condition variable that indicates that one of the workers failed OR that the job completed
    // Repeat until the master tells us the job is done (job_complete is true), at which point we can delete the job
    bool sent_job_failed_msg = false;
    do {
        // We are required to use unsafe locking due to the condition variable
        job_state &state = (*job_states_lock())[job_id].unsafe_get_data();
        std::recursive_mutex &state_lock = (*job_states_lock())[job_id].unsafe_get_mutex();

        bool failed;
        bool job_complete;
        {
            std::unique_lock<std::recursive_mutex> lock(state_lock);
            state.cv_done.wait(lock, [&] {
                return state.job_complete || state.job_failed || !running.load();
            });

            if (!running.load()) {
                return;
            }

            failed = state.job_failed;
            job_complete = state.job_complete;
        }

        // If some portion of the job irrecoverably failed and we haven't told the master yet
        if (failed && !sent_job_failed_msg) {
            notify_job_failed(job_id);
            sent_job_failed_msg = true;
        }

        if (job_complete) {
            // This is safe to call despite state not being locked since threadpools are threadsafe!
            // Wait for all the workers to complete (they should be nearly / already complete at this point)
            state.tp->finish();

            // Delete the exe for this job
            string exe_path = config->get_mj_dir() + "exe_" + std::to_string(job_id);
            pclose(popen(("rm " + exe_path).c_str(), "r"));

            // Delete the job
            unlocked<job_state_map> job_states = job_states_lock();
            job_states->erase(job_id);

            lg->info("Removed job with ID " + std::to_string(job_id));
            break;
        }
    } while (true);
}

void mj_worker_impl::notify_job_failed(int job_id) {
    mj_message failed_msg(hb->get_id(), mj_job_failed{job_id});

    // Loop in order to send the message to the correct master node in the case of master failure
    while (running.load()) {
        string master_hostname = "";
        el->wait_master_node([this, &master_hostname, job_id] (member const& master) {
            master_hostname = master.hostname;
        });

        // TODO: prevent clients from flooding master with requests immediately after election
        std::unique_ptr<tcp_client> client = fac->get_tcp_client(master_hostname, config->get_mj_master_port());
        if (client.get() == nullptr) {
            continue;
        }

        if (client->write_to_server(failed_msg.serialize()) <= 0) {
            continue;
        }

        lg->info("Informed master node that job with ID " + std::to_string(job_id) + " has failed");
        break;
    }
}

void mj_worker_impl::add_files_to_job(int job_id, std::vector<std::string> const& new_files) {
    unlocked<job_state> state{unlocked<job_state>::empty()};
    {
        unlocked<job_state_map> job_states = job_states_lock();
        if (job_states->find(job_id) != job_states->end()) {
            state = (*job_states)[job_id]();
        }
    }

    if (!state) {
        return;
    }

    // Store some values from the state object that the individual threads will use
    processor::type processor_type = state->processor_type;
    string sdfs_output_dir = state->sdfs_output_dir;
    int num_appends_parallel = state->num_appends_parallel;

    for (auto const& filename : new_files) {
        lg->debug("[Job " + std::to_string(job_id) + "] Processing file " + filename);

        // Enqueue a thread to download the file, run the exe on it, and write the results back to SDFS
        string sdfs_src_dir = state->sdfs_src_dir;
        state->tp->enqueue([=] {
            process_file(job_id, filename, sdfs_src_dir, sdfs_output_dir, processor_type, num_appends_parallel);
        });
    }
}

void mj_worker_impl::process_file(int job_id, string const& filename, string const& sdfs_src_dir,
    string const& sdfs_output_dir, processor::type processor_type, int num_appends_parallel)
{
    string sdfs_file_path = sdfs_src_dir + "/" + filename;
    string exe_path = config->get_mj_dir() + "exe_" + std::to_string(job_id);

    // Download the file from the SDFS
    string local_file_path = config->get_mj_dir() + filename + "_" + std::to_string(job_id) + "_" + std::to_string(mt());
    if (!utils::backoff([&] {
            return sdfsc->get(local_file_path, sdfs_file_path) == 0;
        }, [&] {return !running.load();}))
    {
        return;
    }
    lg->trace("[Job " + std::to_string(job_id) + "] Downloaded input file " + filename + " from SDFS");

    // Construct the processor object which will process the output of the command and determine where in SDFS it goes
    std::unique_ptr<processor> proc = processor_factory::get_processor(processor_type);

    // Actually run the program on the input file, processing it line by line
    lg->trace("[Job " + std::to_string(job_id) + "] Running command " + exe_path + " " + local_file_path);
    bool success = run_command(exe_path + " " + local_file_path + " " + filename, [&] (string line) {
        return proc->process_line(line);
    });

    // The provided executable was not valid
    if (!success) {
        // Notify the job monitor that the job is failed
        unlocked<job_state> state = (*job_states_lock())[job_id]();
        state->job_failed = false;
        state->cv_done.notify_all();
        return;
    }

    append_output(job_id, proc.get(), filename, sdfs_output_dir, num_appends_parallel);
    pclose(popen(("rm " + local_file_path).c_str(), "r"));
}

void mj_worker_impl::append_output(int job_id, processor *proc, string const& input_file,
    string const& sdfs_output_dir, int num_appends_parallel)
{
    while (running.load()) {
        // First, get the master hostname from the election service
        string master_hostname = "";
        el->wait_master_node([this, &master_hostname, job_id] (member const& master) {
            lg->trace("[Job " + std::to_string(job_id) + "] Got master node at " + master.hostname + " from election");
            master_hostname = master.hostname;
        });

        std::unique_ptr<tcp_client> client = fac->get_tcp_client(master_hostname, config->get_mj_master_port());
        if (client.get() == nullptr) {
            continue;
        }

        std::unique_ptr<threadpool> tp = tp_fac->get_threadpool(num_appends_parallel);
        std::atomic<bool> master_down = false;

        for (auto const& [output_filename, vals] : *proc) {
            if (master_down.load()) {
                break;
            }

            optional<function<void()>> append_lambda =
                append_lines(job_id, client.get(), input_file, sdfs_output_dir + "/" + output_filename, vals, &master_down);

            if (append_lambda) {
                tp->enqueue(append_lambda.value());
            }
        }

        tp->finish();

        // Restart everything if the master went down
        if (master_down.load()) {
            continue;
        }

        // Quit if we stopped in the middle instead of incorrectly informing master node that we are done
        if (!running.load()) {
            return;
        }

        // Inform the master node that the file is complete
        mj_message complete_msg(hb->get_id(), mj_file_done{job_id, config->get_hostname(), input_file});

        if (client->write_to_server(complete_msg.serialize()) <= 0) {
            continue;
        }
        break;
    }
}

auto mj_worker_impl::append_lines(int job_id, tcp_client *client, string const& input_file,
    string const& output_file_path, std::vector<string> const& vals, std::atomic<bool> *master_down) -> optional<function<void()>>
{
    // Then, construct the message requesting permission to append
    mj_message msg(hb->get_id(), mj_request_append_perm{job_id, config->get_hostname(), input_file, output_file_path});
    string msg_str = msg.serialize();

    // Connect to the master and send the message, and wait for either a yes or no response
    if (client->write_to_server(msg_str) <= 0) {
        lg->trace("[Job " + std::to_string(job_id) + "] Request to append to output file " + output_file_path + " failed to send to master");
        *master_down = true;
        return std::nullopt;
    }

    string response_str = client->read_from_server();

    mj_message response(response_str.c_str(), response_str.length());
    if (!response.is_well_formed() || response.get_msg_type() != mj_message::mj_msg_type::APPEND_PERM) {
        lg->trace("[Job " + std::to_string(job_id) + "] Lost connection with master node while " +
            "requesting permission to append to output file " + output_file_path);
        *master_down = true;
        return std::nullopt;
    }

    bool got_permission = (response.get_msg_data<mj_append_perm>().allowed != 0);
    lg->trace("[Job " + std::to_string(job_id) + "] " + (got_permission ? "Received " : "Did not receive ") +
        "permission to append to output file " + output_file_path);

    if (got_permission) {
        return optional<function<void()>>([=] {
            // Append the values to the output file using sdfs_client
            string append;
            for (auto const& val : vals) {
                append += val + "\n";
            }

            // TODO: create a type for Maplejuice metadata instead of adhoc serialization
            serializer ser;
            ser.add_field(input_file);
            ser.add_field(job_id);
            string metadata_val = ser.serialize();

            // Include the metadata {maplejuice: input_file} so the master can record the append as successful
            utils::backoff([&] {
                bool complete = false;
                return sdfsc->append(inputter<string>([&] () -> optional<string> {
                    if (complete) {
                        return std::nullopt;
                    } else {
                        complete = true;
                        return append;
                    }
                }), output_file_path, {{"maplejuice", metadata_val}}) == 0;
            }, [&] {return !running.load();});

            lg->debug("[Job " + std::to_string(job_id) + "] Appended values to output file " + output_file_path + " from input file " + input_file);
        });
    } else {
        lg->debug("[Job " + std::to_string(job_id) + "] Not appending values to output file " + output_file_path + " from input file " + input_file);
        return std::nullopt;
    }
}

auto mj_worker_impl::run_command(string const& command, function<bool(string const&)> const& callback) const -> bool {
    char buffer[1024];
    FILE *stream = popen(command.c_str(), "r");
    if (stream) {
        string cur_line = "";
        while (!feof(stream)) {
            size_t bytes_read = fread(static_cast<void*>(buffer), 1, 1024, stream);
            if (bytes_read == 1024 || feof(stream)) {
                cur_line += string(buffer, bytes_read);

                while (true) {
                    int newline_pos = -1;
                    for (size_t i = 0; i < cur_line.length(); i++) {
                        if (cur_line.at(i) == '\n') {
                            newline_pos = i;
                            break;
                        }
                    }

                    // Check if we encountered a \n, which means we should call the callback
                    if (newline_pos >= 0) {
                        if (!callback(cur_line.substr(0, newline_pos))) {
                            lg->debug("Callback has indicated failure");
                            pclose(stream);
                            return false;
                        }
                        cur_line = cur_line.substr(newline_pos + 1);
                    } else {
                        break;
                    }
                }
            } else {
                lg->debug("Error occurred while processing output of command");
            }
        }
        pclose(stream);
        return true;
    } else {
        return false;
    }
}

register_auto<mj_worker, mj_worker_impl> register_mj_worker;
