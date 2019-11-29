#include "heartbeater.h"
#include "udp.h"
#include "logging.h"
#include "member_list.h"
#include "test.h"
#include "cli.h"
#include "environment.h"

#include <chrono>
#include <thread>
#include <memory>

using std::unique_ptr;
using std::make_unique;

int main(int argc, char **argv) {
    // Arguments for command: member ...
    std::string local_hostname;
    bool is_first_node;
    int port;
    logger::log_level log_level = logger::log_level::level_off;
    // Arguments for subcommand: member test ...
    std::string test_prefix;

    // Setup CLI arguments and options
    cli_parser.add_required_option("h", "hostname", "The hostname of this node that other nodes can use", &local_hostname);
    cli_parser.add_option("f", "Indicates whether or not we are the first node in the group", &is_first_node);
    cli_parser.add_required_option("p", "port", "The UDP port to use for heartbeating", [&port] (std::string str) {
        try {
            port = std::stoi(str);
            return true;
        } catch (...) {
            return false;
        }
    });
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
    cli_parser.add_option("l", "log_level", "The logging level to use: either OFF, INFO, DEBUG, or TRACE", log_level_parser);

    cli_command *test_parser = cli_parser.add_subcommand("test");
    test_parser->add_option("p", "prefix", "The prefix of the tests to run. By default all tests will be run", &test_prefix);
    test_parser->add_option("l", "log_level", "The logging level to use: either OFF, INFO, DEBUG, or TRACE", log_level_parser);

    // Run CLI parser and exit on failure
    if (!cli_parser.parse("member", argc, argv)) {
        return 1;
    }

    if (test_parser->was_invoked()) {
        testing::run_tests(test_prefix, log_level, log_level != logger::log_level::level_off);
        return 0;
    }

    if (!test_parser->was_invoked()) {
        environment env(false);
        env.get<configuration>()->set_hostname(local_hostname);
        env.get<configuration>()->set_hb_port(port);
        env.get<configuration>()->set_first_node(is_first_node);
        env.get<logger_factory>()->configure(log_level);

        // Start up the heartbeater
        env.get<heartbeater>()->start();

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    return 0;
}
