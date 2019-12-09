#include "heartbeater.h"
#include "udp.h"
#include "logging.h"
#include "member_list.h"
#include "test.h"
#include "cli.h"
#include "environment.h"
#include "election.h"
#include "sdfs_client.h"
#include "sdfs_server.h"
#include "sdfs_master.h"

#include <string>
#include <chrono>
#include <thread>

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
    cli_parser.add_required_option("h", "hostname", "The hostname of this node that other nodes can use", &local_hostname);
    cli_parser.add_option("f", "Indicates whether or not we are the first node in the group", &is_first_node);
    cli_parser.add_option("hp", "port", "The UDP port to use for heartbeating (default 1234)", [&hb_port, &el_port] (std::string str) {
        try {
            hb_port = std::stoi(str);
            return true;
        } catch (...) {
            return false;
        }
    });
    cli_parser.add_option("ep", "port", "The UDP port to use for elections (default 1235)", [&hb_port, &el_port] (std::string str) {
        try {
            el_port = std::stoi(str);
            return true;
        } catch (...) {
            return false;
        }
    });
    cli_parser.add_option("ip", "port", "The TCP port used for communication between nodes in SDFS (default 1234)",
        [&sdfs_internal_port] (std::string str) {
            try {
                sdfs_internal_port = std::stoi(str);
                return true;
            } catch (...) {
                return false;
            }
    });
    cli_parser.add_option("ip", "port", "The TCP port used for communication between clients and the master node in SDFS (default 1235)",
        [&sdfs_master_port] (std::string str) {
            try {
                sdfs_master_port = std::stoi(str);
                return true;
            } catch (...) {
                return false;
            }
    });
    cli_parser.add_required_option("d", "dir", "The empty directory to store any files in, ending with a /", [&dir] (std::string str) {
        if (str[str.length() - 1] != '/') {
            return false;
        }
        dir = str;
        return true;
    });
    cli_parser.add_required_option("sd", "sdfs_subdir", "The name of the subdirectory to store SDFS files in", &sdfs_subdir);

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
    cli_parser.add_option("l", "log_level", "The logging level to use: OFF, INFO, DEBUG, or TRACE (default: OFF)", log_level_parser);

    cli_command *test_parser = cli_parser.add_subcommand("test");
    test_parser->add_option("p", "prefix", "The prefix of the tests to run. By default all tests will be run", &test_prefix);
    test_parser->add_option("l", "log_level", "The logging level to use: OFF, INFO, DEBUG, or TRACE (default OFF)", log_level_parser);

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
        // env.get<sdfs_client>()->start();
        env.get<heartbeater>()->start();
        env.get<election>()->start();
        env.get<sdfs_server>()->start();
        env.get<sdfs_master>()->start();
        // env.get<sdfs_client>()->get_operation("my_file", "test");
        env.get<sdfs_client>()->put_operation("files/md", "test");
        env.get<sdfs_client>()->get_metadata_operation("test");
        // virtual int put_operation(std::string local_filename, std::string sdfs_filename) = 0;
        // virtual int put_operation(int socket, std::string sdfs_filename) = 0;
        // virtual int put_operation_internal(int socket, std::string local_filename, std::string sdfs_filename) = 0;
        // std::thread *client_t = new std::thread([this, client] {handle_connection(client);});

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    return 0;
}
