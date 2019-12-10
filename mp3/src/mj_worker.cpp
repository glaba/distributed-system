#include "mj_worker.h"
#include "mj_worker.hpp"
#include "environment.h"
#include "mj_messages.h"
#include "serialization.h"
#include "member_list.h"

#include <stdlib.h>
#include <algorithm>
#include <unordered_map>
#include <fstream>

#define MAX_NUM_RETRIES 50

using std::string;

mj_worker_impl::mj_worker_impl(environment &env)
    : lg(env.get<logger_factory>()->get_logger("mj_worker"))
    , config(env.get<configuration>())
    , fac(env.get<tcp_factory>())
    , server(env.get<tcp_factory>()->get_tcp_server())
    , hb(env.get<heartbeater>())
    , el(env.get<election>())
    , mm(env.get<mj_master>())
    , sdfsc(env.get<sdfs_client>())
    , sdfss(env.get<sdfs_server>())
    , sdfsm(env.get<sdfs_master>()), running(false), mt(std::chrono::system_clock::now().time_since_epoch().count()) {}

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

    running = false;
    server->stop_server();
    mm->stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    el->stop();
}

void mj_worker_impl::server_thread_function() {
    server->setup_server(config->get_mj_internal_port());

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

            // We only handle messages of the type ASSIGN_JOB
            if (!msg.is_well_formed() || msg.get_msg_type() != mj_message::mj_msg_type::ASSIGN_JOB) {
                lg->debug(msg_str);
                lg->debug("Received malformed message from master, meaning master has most likely crashed");
                return;
            }

            mj_assign_job data = msg.get_msg_data<mj_assign_job>();

            { // Start an atomic block to access the job_states map
                std::lock_guard<std::mutex> guard(job_states_mutex);

                int job_id = data.job_id;
                bool already_running = (job_states.find(job_id) != job_states.end());

                job_states[job_id].exe = data.exe;
                job_states[job_id].sdfs_src_dir = data.sdfs_src_dir;
                job_states[job_id].sdfs_output_dir = data.sdfs_output_dir;
                job_states[job_id].outputter_type = data.outputter_type;
                job_states[job_id].num_files_parallel = data.num_files_parallel;
                job_states[job_id].num_appends_parallel = data.num_appends_parallel;

                // Add all the files in the assignment to the list of files we need to process, ignoring duplicates
                std::vector<string> &files = job_states[job_id].files;
                for (string file : data.input_files) {
                    if (std::find(files.begin(), files.end(), file) == files.end()) {
                        files.push_back(file);
                    }
                }

                // Spin up a thread to process the job if it isn't already running
                if (!already_running) {
                    std::thread job_thread([this, job_id] {run_job(job_id);});
                    job_thread.detach();
                } else {
                    lg->info("Accepting extra work due to loss of a node for job with ID " + std::to_string(job_id));
                }
            }
        });
        client_thread.detach();
    }
}

bool mj_worker_impl::retry(std::function<bool()> callback) {
    unsigned attempts = 0;
    unsigned backoff = std::rand() % 8 + 2;
    while (!callback()) {
        if (attempts++ >= MAX_NUM_RETRIES) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
        backoff *= 1.5;
        if (backoff > 30) {
            backoff = std::rand() % 20 + 30;
        }
    }
    return true;
}

void mj_worker_impl::run_job(int job_id) {
    job_state state;

    { // Safely get the job state
        std::lock_guard<std::mutex> guard(job_states_mutex);
        state = job_states[job_id];
    }

    // Download exe from the SDFS
    string exe_path = config->get_mj_dir() + "exe_" + std::to_string(job_id);
    retry([this, &exe_path, &state] {
        return sdfsc->get_sharded(exe_path, state.exe) == 0;
    });
    FILE *stream = popen(("chmod +x \"" + exe_path + "\"").c_str(), "r");
    pclose(stream);
    lg->debug("[Job " + std::to_string(job_id) + "] Downloaded executable " + state.exe + " from SDFS");

    // An array of threads processing files in parallel, and a boolean that they set to true on failure
    std::mutex file_threads_mutex;
    std::vector<std::optional<std::thread>> file_threads;
    for (int i = 0; i < state.num_files_parallel; i++) {
        file_threads.push_back(std::nullopt);
    }
    std::atomic<bool> failed = false;

    while (running.load()) {
        string cur_file;
        string cur_file_name_only;
        { // Pop an item from the list of input files and exit the loop if we are done
            std::lock_guard<std::mutex> guard(job_states_mutex);
            if (job_states[job_id].files.size() == 0) {
                lg->debug("[Job " + std::to_string(job_id) + "] Done processing files");
                // Remove the entry from the job_states map
                job_states.erase(job_id);
                break;
            }
            cur_file_name_only = job_states[job_id].files.back();
            job_states[job_id].files.pop_back();
            cur_file = job_states[job_id].sdfs_src_dir + cur_file_name_only;
        }
        lg->debug("[Job " + std::to_string(job_id) + "] Processing file " + cur_file);

        // Stop processing files if any of the files has failed
        if (failed.load()) {
            break;
        }

        int thread_index = -1;
        while (true) {
            {
                std::lock_guard<std::mutex> guard(file_threads_mutex);
                for (int i = 0; i < state.num_files_parallel; i++) {
                    if (!file_threads[i]) {
                        thread_index = i;
                        break;
                    }
                }
                if (thread_index != -1) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        assert(thread_index >= 0);

        // Start a thread to download the file, process it, and write the results back to SDFS
        std::thread file_thread([this, job_id, thread_index, cur_file, cur_file_name_only,
            &state, &exe_path, &failed, &file_threads_mutex, &file_threads]
        {
            // Download cur_file from the SDFS
            string cur_file_path = config->get_mj_dir() + cur_file + "_" + std::to_string(job_id);
            retry([this, &cur_file_path, &cur_file] {
                return sdfsc->get_sharded(cur_file_path, cur_file) == 0;
            });
            lg->debug("[Job " + std::to_string(job_id) + "] Downloaded input file " + cur_file + " from SDFS");

            // Construct the outputter object which will process the output of the command and determine where in SDFS it goes
            std::unique_ptr<outputter> outptr = outputter_factory::get_outputter(state.outputter_type, state.sdfs_output_dir);

            // Try to run the program multiple times in case of a spurious failure
            lg->info("[Job " + std::to_string(job_id) + "] Running command " + exe_path + " " + cur_file_path);
            // Actually run the program on the input file, processing it line by line
            bool success = run_command(exe_path + " " + cur_file_path + " " + cur_file_name_only, [this, &outptr, job_id] (string line) {
                return outptr->process_line(line);
            });
            // maple_exe is not a valid executable
            if (!success) {
                return;
            }

            append_output(job_id, outptr.get(), cur_file_name_only, state.num_appends_parallel);

            // Detach ourselves and remove ourselves from the file_threads map
            std::lock_guard<std::mutex> guard(file_threads_mutex);
            file_threads[thread_index].value().detach();
            file_threads[thread_index].reset();
        });
        file_threads[thread_index] = std::move(file_thread);
    }

    for (int i = 0; i < state.num_files_parallel; i++) {
        if (file_threads[i]) {
            file_threads[i].value().join();
            file_threads[i].reset();
        }
    }
}

void mj_worker_impl::append_output(int job_id, outputter *outptr, string input_file, int num_appends_parallel)
{
    while (running.load()) {
        // First, get the master hostname from the election service
        string master_hostname = "";
        el->wait_master_node([this, &master_hostname, job_id] (member master) {
            lg->trace("[Job " + std::to_string(job_id) + "] Got master node at " + master.hostname + " from election");
            master_hostname = master.hostname;
        });

        std::unique_ptr<tcp_client> client = fac->get_tcp_client();
        int fd = client->setup_connection(master_hostname, config->get_mj_master_port());
        try {
            if (fd < 0) {
                throw client.get();
            }

            // Fire off threads for each key that needs to be appended
            std::vector<std::optional<std::thread>> append_threads;
            for (int i = 0; i < num_appends_parallel; i++) {
                append_threads.push_back(std::nullopt);
            }
            int num_append_threads = 0;
            std::atomic<bool> master_down = false;

            while (outptr->more()) {
                auto pair = outptr->emit();
                string &output_file = pair.first;
                std::vector<string> &vals = pair.second;

                if (master_down.load()) {
                    break;
                }

                if (num_append_threads == num_appends_parallel) {
                    for (int i = 0; i < num_appends_parallel; i++) {
                        append_threads[i].value().join();
                        append_threads[i].reset();
                        num_append_threads--;
                    }
                }

                unsigned thread_index = -1;
                for (int i = 0; i < num_appends_parallel; i++) {
                    if (!append_threads[i]) {
                        thread_index = i;
                    }
                }
                assert(thread_index >= 0);

                std::optional<std::thread> append_thread =
                    append_lines(job_id, client.get(), fd, input_file, output_file, vals, &master_down);

                if (append_thread) {
                    append_threads[thread_index] = std::move(append_thread.value());
                    num_append_threads++;
                }
            }

            // Wait for all appends to complete and restart everything if the master went down
            for (int i = 0; i < num_appends_parallel; i++) {
                if (append_threads[i]) {
                    append_threads[i].value().join();
                    append_threads[i].reset();
                }
            }
            if (master_down.load()) {
                throw client.get();
            }

            // Inform the master node that the file is complete
            mj_message complete_msg(hb->get_id(), mj_file_done{job_id, config->get_hostname(), input_file});

            lg->debug("[Job " + std::to_string(job_id) + "] Informing master that file " + input_file + " is complete");
            if (client->write_to_server(fd, complete_msg.serialize()) <= 0) {
                throw client.get();
            }
            client->close_connection(fd);
            break;
        } catch (tcp_client *failed_client) {
            failed_client->close_connection(fd);
            continue;
        }
    }
}

std::optional<std::thread> mj_worker_impl::append_lines(int job_id, tcp_client *client, int fd, string &input_file,
    string &output_file, std::vector<string> &vals, std::atomic<bool> *master_down)
{
    // Then, construct the message request permission to append
    mj_message msg(hb->get_id(), mj_request_append_perm{job_id, config->get_hostname(), input_file, output_file});
    string msg_str = msg.serialize();

    // Connect to the master and send the message, and wait for either a yes or no response
    if (client->write_to_server(fd, msg_str) <= 0) {
        lg->debug("[Job " + std::to_string(job_id) + "] Request to append to output file " + output_file + " failed to send to master");
        *master_down = true;
        return std::nullopt;
    }

    string response_str = client->read_from_server(fd);

    mj_message response(response_str.c_str(), response_str.length());
    if (!response.is_well_formed() || response.get_msg_type() != mj_message::mj_msg_type::APPEND_PERM) {
        lg->debug("[Job " + std::to_string(job_id) + "] Lost connection with master node while " +
            "requesting permission to append to output file " + output_file);
        *master_down = true;
        return std::nullopt;
    }

    bool got_permission = (response.get_msg_data<mj_append_perm>().allowed != 0);
    lg->debug("[Job " + std::to_string(job_id) + "] " + (got_permission ? "Received " : "Did not receive ") +
        "permission to append to output file " + output_file);

    if (got_permission) {
        std::thread append_thread([=] {
            // First, put the values into a local file
            unsigned cur_id = mt();
            string local_output = config->get_mj_dir() + "intermediate_" + std::to_string(cur_id);

            retry([&] {
                std::ofstream out;
                out.open(local_output);
                if (!out) {
                    return false;
                }

                for (const string &val : vals) {
                    out << val << std::endl;
                }
                out.close();

                return out.good();
            });
            lg->trace("[Job " + std::to_string(job_id) + "] Wrote intermediate results for output file " + output_file + " to local file");

            // Then, append the values to file with name intermediate_filename using sdfs_client
            retry([this, &local_output, &output_file] {
                return sdfsc->append_operation(local_output, output_file) == 0;
            });
            pclose(popen(string("rm \"" + local_output + "\"").c_str(), "r"));

            // TODO: add metadata key=maple, value=input_file to SDFS entry

            lg->debug("[Job " + std::to_string(job_id) + "] Appended values to output file " + output_file + " from input file " + input_file);
        });
        return std::optional<std::thread>(std::move(append_thread));
    } else {
        lg->info("[Job " + std::to_string(job_id) + "] Not appending values to output file " + output_file + " from input file " + input_file);
        return std::nullopt;
    }
}

bool mj_worker_impl::run_command(string command, std::function<bool(string)> callback) {
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
