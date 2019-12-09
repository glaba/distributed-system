#include "cli.h"
#include "logging.h"
#include "environment.h"
#include "configuration.h"
#include "mj_worker.h"
#include "test.h"
#include "heartbeater.h"
#include "sdfs_client.h"

#include <string>
#include <chrono>
#include <thread>
#include <functional>

using std::string;
using callback = std::function<bool(string)>;
using none = std::monostate;

int main(int argc, char **argv) {
    // Arguments for command maplejuice ...
    std::string local_hostname;
    string introducer;
    int hb_port;
    int el_port;
    std::string dir;
    std::string sdfs_subdir;
    std::string mj_subdir;
    int sdfs_internal_port;
    int sdfs_master_port;
    int mj_internal_port;
    int mj_master_port;
    logger::log_level log_level = logger::log_level::level_off;
    // Arguments for command maplejuice test ...
    int parallelism;
    string test_prefix;

    cli_parser.add_required_option<>("h", "hostname", "The hostname of this node that other nodes can use", &local_hostname);
    cli_parser.add_option<>("i", "introducer", "The hostname of a node already in the group, or none if we are the first member", &introducer);
    cli_parser.add_option<>("hp", "port", "The UDP port to use for heartbeating", &hb_port, 1234);
    cli_parser.add_option<>("ep", "port", "The UDP port to use for elections", &el_port, 1235);
    cli_parser.add_option<>("sip", "port", "The TCP port used for communication between nodes in SDFS", &sdfs_internal_port, 1234);
    cli_parser.add_option<>("smp", "port", "The TCP port used for communication between clients and the master node in SDFS", &sdfs_master_port, 1235);
    cli_parser.add_option<>("mip", "port", "The TCP port used for communication with Maple nodes", &mj_internal_port, 1236);
    cli_parser.add_option<>("mmp", "port", "The TCP port used for communication with the Maple master", &mj_master_port, 1237);
    std::function<bool(std::string)> dir_validator = [&dir] (std::string str) {
        if (str[str.length() - 1] != '/') {
            return false;
        }
        dir = str;
        return true;
    };
    cli_parser.add_required_option<>("d", "dir", "The empty directory to store any files in, ending with a /", &dir_validator);
    cli_parser.add_required_option<>("sd", "sdfs_subdir", "The name of the subdirectory to store SDFS files in", &sdfs_subdir);
    cli_parser.add_required_option<>("md", "mj_subdir", "The name of the subdirectory to store temporary MapleJuice files in", &mj_subdir);

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

    cli_command *test_parser = cli_parser.add_subcommand("test");
    test_parser->add_option<>("j", "parallelism", "The number of threads to use if there is no logging", &parallelism, 1);
    test_parser->add_option<>("p", "prefix", "The prefix of the tests to run. By default all tests will be run", &test_prefix);
    test_parser->add_option<callback, none>("l", "log_level", "The logging level to use: either OFF, INFO, DEBUG, or TRACE", &log_level_parser);

    // Run CLI parser and exit on failure
    if (!cli_parser.parse("maplejuice", argc, argv)) {
        return 1;
    }

    if (test_parser->was_invoked()) {
        testing::run_tests(test_prefix, log_level, parallelism, false);
        return 0;
    }

    if (!test_parser->was_invoked()) {
        environment env(false);

        configuration *config = env.get<configuration>();
        config->set_hostname(local_hostname);
        config->set_first_node(introducer == "");
        config->set_hb_port(hb_port);
        config->set_election_port(el_port);
        config->set_dir(dir);
        config->set_sdfs_subdir(sdfs_subdir);
        config->set_sdfs_internal_port(sdfs_internal_port);
        config->set_sdfs_master_port(sdfs_master_port);
        config->set_mj_subdir(mj_subdir);
        config->set_mj_internal_port(mj_internal_port);
        config->set_mj_master_port(mj_master_port);

        env.get<logger_factory>()->configure(log_level);

        env.get<mj_worker>()->start();
        if (introducer != "") {
            env.get<heartbeater>()->join_group(introducer);
        }

        sdfs_client *sdfsc = env.get<sdfs_client>();
        while (true) {
            // Command line to put and get files in SDFS
            std::cout << "> " << std::flush;

            string command, local_filename, sdfs_filename;
            std::cin >> command;
            std::cin >> local_filename;
            std::cin >> sdfs_filename;

            if (command == "put") {
                sdfsc->put_operation(local_filename, sdfs_filename + ".0");
            } else if (command == "get") {
                if (sdfsc->get_sharded(local_filename, sdfs_filename) != 0) {
                    std::cout << "Get failed" << std::endl;
                }
            } else if (command == "append") {
                sdfsc->append_operation(local_filename, sdfs_filename);
            } else {
                std::cout << "Usage: put/get/append <local_filename> <sdfs_filename>" << std::endl;
            }
        }
    }
}
