#include "cli.h"
#include "logging.h"
#include "environment.h"
#include "configuration.h"
#include "juice_client.h"
#include "partitioner.h"

#include <string>
#include <chrono>
#include <thread>
#include <functional>

using std::string;
using callback = std::function<bool(string)>;
using none = std::monostate;

int main(int argc, char **argv) {
    // Arguments for command juice ...
    string local_exe;
    string juice_exe;
    int num_juices;
    string sdfs_intermediate_filename_prefix;
    string sdfs_dest_filename;
    string juice_master;
    int sdfs_internal_port;
    int sdfs_master_port;
    int juice_master_port;
    logger::log_level log_level = logger::log_level::level_off;
    partitioner::type partitioner_type = partitioner::type::round_robin;

    cli_parser.add_argument<>("local_exe", "The path to the executable which performs map on individual files", &local_exe);
    cli_parser.add_argument<>("juice_exe", "The name the executable will be stored under in SDFS", &juice_exe);
    cli_parser.add_argument<>("num_juices", "The number of Juice tasks to run", &num_juices);
    cli_parser.add_argument<>("sdfs_intermediate_filename_prefix", "The prefix for intermediate files emitted by Maple", &sdfs_intermediate_filename_prefix);
    cli_parser.add_argument<>("sdfs_dest_filename", "The file to put output in", &sdfs_dest_filename);
    cli_parser.add_required_option<>("h", "juice_master", "The hostname of the master node in the cluster", &juice_master);
    cli_parser.add_option<>("ip", "port", "The TCP port used for communication between nodes in SDFS", &sdfs_internal_port, 1234);
    cli_parser.add_option<>("mp", "port", "The TCP port used for communication between clients and the master node in SDFS", &sdfs_master_port, 1235);
    cli_parser.add_option<>("p", "port", "The TCP port used for communication with the Juice master", &juice_master_port, 1237);

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
    cli_parser.add_option<callback, none>("l", "log_level", "The logging level to use: OFF, INFO, DEBUG, or TRACE (default: OFF)", &log_level_parser);

    std::function<bool(std::string)> partitioner_parser = [&partitioner_type] (std::string str) {
        if (str == "round_robin") {
            partitioner_type = partitioner::type::round_robin;
        } else if (str == "hash") {
            partitioner_type = partitioner::type::hash;
        } else if (str == "range") {
            partitioner_type = partitioner::type::range;
        } else {
            return false;
        }
        return true;
    };
    cli_parser.add_option<callback, none>("r", "partitioner", "The partitioner to use: round_robin, hash, or range (default: round_robin", &partitioner_parser);

    // Run CLI parser and exit on failure
    if (!cli_parser.parse("juice", argc, argv)) {
        return 1;
    }

    environment env(false);

    configuration *config = env.get<configuration>();
    config->set_sdfs_internal_port(sdfs_internal_port);
    config->set_sdfs_master_port(sdfs_master_port);
    config->set_mj_master_port(juice_master_port);

    env.get<logger_factory>()->configure(log_level);

    if (env.get<juice_client>()->run_job(juice_master, local_exe, juice_exe, num_juices, partitioner_type, sdfs_intermediate_filename_prefix, sdfs_dest_filename)) {
        std::cout << "Juice job successfully completed" << std::endl;
    } else {
        std::cout << "Juice job failed to complete: " + env.get<juice_client>()->get_error() << std::endl;
    }
}
