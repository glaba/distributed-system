#include "cli.h"
#include "logging.h"
#include "environment.h"
#include "configuration.h"
#include "test.h"

#include <string>
#include <chrono>
#include <thread>
#include <functional>

using std::string;
using callback = std::function<bool(string)>;

int main(int argc, char **argv) {
    // Arguments for command: sdfs ...
    std::string local_hostname;
    bool is_first_node;
    int hb_port = 1234;
    int el_port = 1235;
    int sdfs_internal_port = 1234;
    int sdfs_master_port = 1235;
    std::string dir;
    std::string sdfs_subdir;
    logger::log_level log_level = logger::log_level::level_off;
    // Arguments for subcommand: sdfs test ...
    std::string test_prefix;

    // Setup CLI arguments and options
    cli_parser.add_required_option<string>("h", "hostname", "The hostname of this node that other nodes can use", &local_hostname);
    cli_parser.add_option<bool>("f", "first_node", "Indicates whether or not we are the first node in the group", &is_first_node);
    cli_parser.add_option<int>("hp", "port", "The UDP port to use for heartbeating (default 1234)", &hb_port);
    cli_parser.add_option<int>("ep", "port", "The UDP port to use for elections (default 1235)", &el_port);
    cli_parser.add_option<int>("ip", "port", "The TCP port used for communication between nodes in SDFS (default 1234)", &sdfs_internal_port);
    cli_parser.add_option<int>("mp", "port", "The TCP port used for communication between clients and the master node in SDFS (default 1235)", &sdfs_master_port);
    std::function<bool(std::string)> dir_validator = [&dir] (std::string str) {
        if (str[str.length() - 1] != '/') {
            return false;
        }
        dir = str;
        return true;
    };
    cli_parser.add_required_option<callback>("d", "dir", "The empty directory to store any files in, ending with a /", &dir_validator);
    cli_parser.add_required_option<string>("sd", "sdfs_subdir", "The name of the subdirectory to store SDFS files in", &sdfs_subdir);

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

    cli_command *test_parser = cli_parser.add_subcommand("test");
    test_parser->add_option<string>("p", "prefix", "The prefix of the tests to run. By default all tests will be run", &test_prefix);
    test_parser->add_option<callback>("l", "log_level", "The logging level to use: OFF, INFO, DEBUG, or TRACE (default OFF)", &log_level_parser);

    // Run CLI parser and exit on failure
    if (!cli_parser.parse("sdfs", argc, argv)) {
        return 1;
    }

    if (test_parser->was_invoked()) {
        testing::run_tests(test_prefix, log_level, log_level != logger::log_level::level_off);
        return 0;
    }

    if (!test_parser->was_invoked()) {
        environment env(false);
        configuration *config = env.get<configuration>();
        config->set_hostname(local_hostname);
        config->set_first_node(is_first_node);
        config->set_hb_port(hb_port);
        config->set_election_port(el_port);
        config->set_sdfs_internal_port(sdfs_internal_port);
        config->set_sdfs_master_port(sdfs_master_port);
        config->set_dir(dir);
        config->set_sdfs_subdir(sdfs_subdir);

        env.get<logger_factory>()->configure(log_level);

        // Start up SDFS services

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    return 0;
}
