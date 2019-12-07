#include "maple_node.h"
#include "maple_node.hpp"
#include "environment.h"
#include "maple_messages.h"
#include "serialization.h"
#include "member_list.h"

#include <stdlib.h>
#include <algorithm>
#include <unordered_map>

#define MAX_NUM_RETRIES 5

using std::string;

maple_node_impl::maple_node_impl(environment &env)
    : sdfsc(nullptr /* Left as nullptr until sdfs_client is implemented */)
    , lg(env.get<logger_factory>()->get_logger("maple_node"))
    , config(env.get<configuration>())
    , client(env.get<tcp_factory>()->get_tcp_client())
    , server(env.get<tcp_factory>()->get_tcp_server())
    , el(env.get<election>())
    , mm(env.get<maple_master>()) {}

void maple_node_impl::start() {
    lg->info("Starting Maple node");
    el->start();
    mm->start();
    running = true;

    std::thread server_thread([this] {server_thread_function();});
    server_thread.detach();
}

void maple_node_impl::stop() {
    running = false;
    server->stop_server();
}

void maple_node_impl::server_thread_function() {
    server->setup_server(config->get_maple_internal_port());

    while (true) {
        // Wait for the master node to connect to us and assign us work
        int fd = server->accept_connection();

        if (!running.load()) {
            lg->debug("Exiting server");
        }

        if (fd < 0) {
            continue;
        }

        std::thread client_thread([this, fd] {
            // Get the message from the master and parse it
            string msg_str = server->read_from_client(fd);
            maple_message msg(msg_str.c_str(), msg_str.length());

            // We only handle messages of the type ASSIGN_JOB
            if (!msg.is_well_formed() || msg.get_msg_type() != maple_message::maple_msg_type::ASSIGN_JOB) {
                lg->debug("Received malformed message from master, meaning master has most likely crashed");
                return;
            }

            maple_assign_job data = msg.get_msg_data<maple_assign_job>();

            { // Start an atomic block to access the job_states map
                std::lock_guard<std::mutex> guard(job_states_mutex);

                int job_id = data.job_id;
                bool already_running = (job_states.find(job_id) != job_states.end());

                job_states[job_id].maple_exe = data.maple_exe;
                job_states[job_id].sdfs_intermediate_filename_prefix = data.sdfs_intermediate_filename_prefix;

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
                }
            }

            // Close the connection with the client
            server->close_connection(fd);
        });
        client_thread.detach();
    }
}

void maple_node_impl::run_job(int job_id) {
    job_state state;

    { // Safely get the job state
        std::lock_guard<std::mutex> guard(job_states_mutex);
        state = job_states[job_id];
    }

    // TODO: download maple_exe from the SDFS
    string maple_exe_path = "";

    while (true) {
        string cur_file;
        { // Pop an item from the list of input files and exit the loop if we are done
            std::lock_guard<std::mutex> guard(job_states_mutex);
            if (job_states[job_id].files.size() == 0) {
                // Remove the entry from the job_states map
                job_states.erase(job_id);
                break;
            }
            cur_file = job_states[job_id].files.back();
            job_states[job_id].files.pop_back();
        }

        // TODO: download cur_file from the SDFS
        string cur_file_path = "";

        // TODO: For now, store key value pairs outputted by maple_exe in memory
        std::unordered_map<string, std::vector<string>> kv_pairs;

        lg->info("[Job " + std::to_string(job_id) + "] Running command " + maple_exe_path + " " + cur_file_path);
        unsigned retry_count = 0;
        while (!run_command(maple_exe_path + " " + cur_file_path, [this, &kv_pairs, job_id] (string line) {
                // The input has the format "<key> <value>", where <key> doesn't contain spaces and <value> doesn't contain \n
                size_t space_index = line.find(" ");
                if (space_index == string::npos) {
                    lg->debug("[Job " + std::to_string(job_id) + "] Invalid format returned by executable");
                    return false;
                }
                string key = line.substr(0, space_index);
                string value = line.substr(space_index + 1);
                kv_pairs[key].push_back(value);
                return true;
            }))
        {
            kv_pairs.clear();

            if (retry_count++ >= MAX_NUM_RETRIES) {
                lg->info("[Job " + std::to_string(job_id) + "] Giving up on job " + std::to_string(job_id) + " after " +
                    std::to_string(MAX_NUM_RETRIES) + " failed attempts to run \"" + maple_exe_path + " " + cur_file_path + "\", exiting");
                return;
            }
            lg->debug("[Job " + std::to_string(job_id) + "] Failed to run program on input file " + cur_file_path + ", retrying");
        }

        for (auto const &[key, vals] : kv_pairs) {
            // Ask maple_master for permission to append
            bool got_permission = false;

            while (true) {
                // First, get the master hostname from the election service
                string master_hostname = "";
                el->wait_master_node([this, &master_hostname, job_id] (member master) {
                    lg->debug("[Job " + std::to_string(job_id) + "] Got master node at " + master.hostname + " from election");
                    master_hostname = master.hostname;
                });

                // Then, construct the message request permission to append
                maple_message msg(maple_request_append_perm{job_id, config->get_hostname(), cur_file, key});
                string msg_str = msg.serialize();

                // Connect to the master and send the message, and wait for either a yes or no response
                int fd = client->setup_connection(master_hostname, config->get_maple_master_port());
                if (fd < 0 || client->write_to_server(fd, msg_str) <= 0) {
                    lg->debug("[Job " + std::to_string(job_id) + "] Request to append key " + key + " failed to send to master");
                    continue; // Master has gone down, poll on election
                }

                string response_str = client->read_from_server(fd);

                maple_message response(response_str.c_str(), response_str.length());
                if (!response.is_well_formed() || response.get_msg_type() != maple_message::maple_msg_type::APPEND_PERM) {
                    lg->debug("[Job " + std::to_string(job_id) + "] Lost connection with master node while " +
                        "requesting permission to append key " + key);
                    continue; // Master has gone down, poll on election
                }

                got_permission = (response.get_msg_data<maple_append_perm>().allowed != 0);
            }

            if (got_permission) {
                string intermediate_filename = state.sdfs_intermediate_filename_prefix + "_" + key;

                // TODO: Append values to file with name intermediate_filename using sdfs_client
                //  with metadata key=maple, value=cur_file

                lg->info("Appended values for key " + key + " to SDFS in file " + intermediate_filename);
            } else {
                lg->info("Not appending values for key " + key);
            }
        }

        while (true) {
            string master_hostname = "";
            el->wait_master_node([&master_hostname] (member master) {
                master_hostname = master.hostname;
            });

            // Inform the master node that the file is complete
            maple_message msg(maple_file_done{job_id, config->get_hostname(), cur_file});

            int fd = client->setup_connection(master_hostname, config->get_maple_master_port());
            if (fd < 0 || client->write_to_server(fd, msg.serialize()) <= 0) {
                client->close_connection(fd);

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue; // Master node has gone down or is not ready yet
            }
            client->close_connection(fd);
            break;
        }
    }
}

bool maple_node_impl::run_command(string command, std::function<bool(string)> callback) {
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
        return true;
    } else {
        return false;
    }
}

register_auto<maple_node, maple_node_impl> register_maple_node;
