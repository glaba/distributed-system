#include "cli.h"
#include "logging.h"
#include "environment.h"
#include "configuration.h"
#include "maple_client.h"

#include <string>
#include <chrono>
#include <thread>
#include <functional>

using std::string;
using callback = std::function<bool(string)>;

int main(int argc, char **argv) {
    // Arguments for command maple ...
    string maple_exe;
    int num_maples;
    string sdfs_intermediate_filename_prefix;
    string sdfs_src_dir;
    string maple_master;
    int sdfs_internal_port = 1234;
    int sdfs_master_port = 1235;
    int maple_port = 1236;
    logger::log_level log_level = logger::log_level::level_off;

    cli_parser.add_argument<string>("maple_exe", "The path to the executable which performs map on individual files", &maple_exe);
    cli_parser.add_argument<int>("num_maples", "The number of Maple tasks to run", &num_maples);
    cli_parser.add_argument<string>("sdfs_intermediate_filename_prefix", "The prefix for intermediate files emitted by Maple", &sdfs_intermediate_filename_prefix);
    cli_parser.add_argument<string>("sdfs_src_dir", "The prefix of files in SDFS to be processed", &sdfs_src_dir);
    cli_parser.add_required_option<string>("h", "maple_master", "The hostname of the master node in the cluster", &maple_master);
    cli_parser.add_option<int>("ip", "port", "The TCP port used for communication between nodes in SDFS (default 1234)", &sdfs_internal_port);
    cli_parser.add_option<int>("mp", "port", "The TCP port used for communication between clients and the master node in SDFS (default 1235)", &sdfs_master_port);
    cli_parser.add_option<int>("p", "port", "The TCP port used for communication with the Maple master (default 1236)", &maple_port);

    std::function<bool(std::string)> log_level_parser = [&log_level] (std::string str) {
        if (str == "OFF") {
            log_level = logger::log_level::level_off;
        } else if (str == "INFO") {
            log_level = logger::log_level::level_info;
        } else if (str == "DEBUG") {
            log_level = logger::log_level::level_debug;
        } else if (str == "TRACE") {
            log_level = logger::log_level::level_trace;
        } else {
            return false;
        }
        return true;
    };
    cli_parser.add_option<callback>("l", "log_level", "The logging level to use: OFF, INFO, DEBUG, or TRACE (default: OFF)", &log_level_parser);

    // Run CLI parser and exit on failure
    if (!cli_parser.parse("maple", argc, argv)) {
        return 1;
    }

    environment env(false);

    configuration *config = env.get<configuration>();
    config->set_sdfs_internal_port(sdfs_internal_port);
    config->set_sdfs_master_port(sdfs_master_port);
    config->set_maple_port(maple_port);

    env.get<logger_factory>()->configure(log_level);
    if (env.get<maple_client>()->run_job(maple_master, maple_exe, num_maples, sdfs_intermediate_filename_prefix, sdfs_src_dir)) {
        std::cout << "Maple job successfully completed" << std::endl;
    } else {
        std::cout << "Maple job failed to complete" << std::endl;
    }
}
