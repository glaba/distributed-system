#include "maple_node.h"
#include "maple_node.hpp"
#include "environment.h"
#include "maple_messages.h"
#include "serialization.h"
#include "member_list.h"

#include <stdlib.h>
#include <algorithm>
#include <unordered_map>
#include <fstream>

#define MAX_NUM_RETRIES 5

using std::string;

maple_node_impl::maple_node_impl(environment &env)
    : lg(env.get<logger_factory>()->get_logger("maple_node"))
    , config(env.get<configuration>())
    , fac(env.get<tcp_factory>())
    , server(env.get<tcp_factory>()->get_tcp_server())
    , hb(env.get<heartbeater>())
    , el(env.get<election>())
    , mm(env.get<maple_master>())
    , sdfsc(env.get<sdfs_client>())
    , sdfss(env.get<sdfs_server>()), running(false) {}

void maple_node_impl::start() {
    if (running.load()) {
        return;
    }

    lg->info("Starting Maple node");
    el->start();
    mm->start();
    sdfsc->start();
    sdfss->start(); // This should start sdfs_master as well
    running = true;

    std::thread server_thread([this] {server_thread_function();});
    server_thread.detach();
}

void maple_node_impl::stop() {
    if (!running.load()) {
        return;
    }

    running = false;
    server->stop_server();
    mm->stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    el->stop();
}

void maple_node_impl::server_thread_function() {
    server->setup_server(config->get_maple_internal_port());

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

            maple_message msg(msg_str.c_str(), msg_str.length());

            // We only handle messages of the type ASSIGN_JOB
            if (!msg.is_well_formed() || msg.get_msg_type() != maple_message::maple_msg_type::ASSIGN_JOB) {
                lg->debug(msg_str);
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
                } else {
                    lg->info("Accepting extra work due to loss of a node for job with ID " + std::to_string(job_id));
                }
            }
        });
        client_thread.detach();
    }
}

bool maple_node_impl::retry(std::function<bool()> callback, int job_id, string description) {
    unsigned attempts = 0;
    while (!callback()) {
        if (attempts++ >= MAX_NUM_RETRIES) {
            lg->info("[Job " + std::to_string(job_id) + "] Giving up " + std::to_string(job_id) + " after " +
                std::to_string(MAX_NUM_RETRIES) + " failed attempts to " + description + ", exiting");
            return false;
        }
        lg->debug("[Job " + std::to_string(job_id) + "] Failed to " + description + ", retrying");
    }
    return true;
}

void maple_node_impl::run_job(int job_id) {
    job_state state;

    { // Safely get the job state
        std::lock_guard<std::mutex> guard(job_states_mutex);
        state = job_states[job_id];
    }

    // Download maple_exe from the SDFS
    string maple_exe_path = config->get_maple_dir() + "maple_exe_" + std::to_string(job_id);
    if (!retry([this, &maple_exe_path, &state] {return sdfsc->get_operation(maple_exe_path, state.maple_exe) == 0;},
        job_id, "get \"" + state.maple_exe + "\" from SDFS"))
    {
        return;
    }
    lg->debug("[Job " + std::to_string(job_id) + "] Downloaded executable " + state.maple_exe + " from SDFS");

    while (running.load()) {
        string cur_file;
        { // Pop an item from the list of input files and exit the loop if we are done
            std::lock_guard<std::mutex> guard(job_states_mutex);
            if (job_states[job_id].files.size() == 0) {
                lg->debug("[Job " + std::to_string(job_id) + "] Done processing files");
                // Remove the entry from the job_states map
                job_states.erase(job_id);
                break;
            }
            cur_file = job_states[job_id].files.back();
            job_states[job_id].files.pop_back();
        }
        lg->debug("[Job " + std::to_string(job_id) + "] Processing file " + cur_file);

        // Download cur_file from the SDFS
        string cur_file_path = config->get_maple_dir() + cur_file + "_" + std::to_string(job_id);
        if (!retry([this, &cur_file_path, &cur_file] {return sdfsc->get_operation(cur_file_path, cur_file) == 0;},
            job_id, "get input file \"" + cur_file + "\" from SDFS"))
        {
            return;
        }
        lg->debug("[Job " + std::to_string(job_id) + "] Downloaded input file " + cur_file + " from SDFS");

        std::unordered_map<string, std::vector<string>> kv_pairs;

        lg->info("[Job " + std::to_string(job_id) + "] Running command " + maple_exe_path + " " + cur_file_path);
        // Try to run the program multiple times in case of a spurious failure
        bool successfully_processed_file = retry([this, &maple_exe_path, &cur_file_path, &kv_pairs, job_id] {
            // Actually run the program on the input file, processing it line by line
            bool success = run_command(maple_exe_path + " " + cur_file_path, [this, &kv_pairs, job_id] (string line) {
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
            });
            if (!success) {
                kv_pairs.clear();
            }
            return success;
        }, job_id, "run program on input file " + cur_file + ", retrying");
        // Fail if we are unable to run maple_exe
        if (!successfully_processed_file) {
            return;
        }

        if (!append_kv_pairs(job_id, kv_pairs, state.sdfs_intermediate_filename_prefix, cur_file)) {
            return;
        }
    }
}

bool maple_node_impl::append_kv_pairs(int job_id, std::unordered_map<string, std::vector<string>> &kv_pairs,
    string sdfs_intermediate_filename_prefix, string filename)
{
    while (running.load()) {
        // First, get the master hostname from the election service
        string master_hostname = "";
        el->wait_master_node([this, &master_hostname, job_id] (member master) {
            lg->trace("[Job " + std::to_string(job_id) + "] Got master node at " + master.hostname + " from election");
            master_hostname = master.hostname;
        });

        std::unique_ptr<tcp_client> client = fac->get_tcp_client();
        int fd = client->setup_connection(master_hostname, config->get_maple_master_port());
        try {
            if (fd < 0) {
                throw client.get();
            }

            for (auto it = kv_pairs.begin(); it != kv_pairs.end(); /* No increment because we are deleting while iterating */) {
                string key = it->first;
                std::vector<string> vals = it->second;

                // Then, construct the message request permission to append
                maple_message msg(hb->get_id(), maple_request_append_perm{job_id, config->get_hostname(), filename, key});
                string msg_str = msg.serialize();

                // Connect to the master and send the message, and wait for either a yes or no response
                if (client->write_to_server(fd, msg_str) <= 0) {
                    lg->debug("[Job " + std::to_string(job_id) + "] Request to append key " + key + " failed to send to master");
                    throw client.get(); // Master has gone down
                }

                string response_str = client->read_from_server(fd);

                maple_message response(response_str.c_str(), response_str.length());
                if (!response.is_well_formed() || response.get_msg_type() != maple_message::maple_msg_type::APPEND_PERM) {
                    lg->debug("[Job " + std::to_string(job_id) + "] Lost connection with master node while " +
                        "requesting permission to append key " + key);
                    throw client.get();
                }

                bool got_permission = (response.get_msg_data<maple_append_perm>().allowed != 0);
                lg->debug("[Job " + std::to_string(job_id) + "] " + (got_permission ? "Received " : "Did not receive ") +
                    "permission to append key " + key);

                if (got_permission) {
                    string intermediate_filename = sdfs_intermediate_filename_prefix + "_" + key;

                    // First, put the values into a local file
                    string local_output = config->get_maple_dir() + "intermediate_" + std::to_string(job_id);
                    std::ofstream out;
                    out.open(local_output);
                    if (!out) {
                        std::cerr << "Failed to open local file to write intermediate results" << std::endl;
                        exit(1);
                    }

                    for (const string &val : vals) {
                        out << val << std::endl;
                    }
                    out.close();
                    lg->trace("[Job " + std::to_string(job_id) + "] Wrote intermediate results for key " + key + " to local file");

                    // Then, append the values to file with name intermediate_filename using sdfs_client
                    if (!retry([this, &local_output, &intermediate_filename] {return sdfsc->append_operation(local_output, intermediate_filename) == 0;},
                        job_id, "append values for key " + key + " from input file " + filename))
                    {
                        pclose(popen(string("rm \"" + local_output + "\"").c_str(), "r"));
                        client->close_connection(fd);
                        return false;
                    }
                    pclose(popen(string("rm \"" + local_output + "\"").c_str(), "r"));

                    // TODO: add metadata key=maple, value=filename to SDFS entry

                    it = kv_pairs.erase(it);
                    lg->debug("[Job " + std::to_string(job_id) + "] Appended values for key " + key + " to SDFS in file " + intermediate_filename);
                } else {
                    ++it;
                    lg->debug("[Job " + std::to_string(job_id) + "] Not appending values for key " + key);
                }
            }

            // Inform the master node that the file is complete
            maple_message complete_msg(hb->get_id(), maple_file_done{job_id, config->get_hostname(), filename});

            lg->debug("[Job " + std::to_string(job_id) + "] Informing master that file " + filename + " is complete");
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
    return true;
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

register_auto<maple_node, maple_node_impl> register_maple_node;
