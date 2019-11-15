#include "member.h"
#include "heartbeater.h"
#include "udp.h"
#include "logging.h"
#include "member_list.h"
#include "test.h"
#include "cli.h"

#include <chrono>
#include <thread>
#include <memory>

using std::unique_ptr;
using std::make_unique;

int main(int argc, char **argv) {
    // Arguments for command: member ...
    std::string local_hostname;
    std::string introducer;
    uint16_t port;
    logger::log_level log_level;
    // Arguments for subcommand: member test ...
    std::string test_prefix;

    // Setup CLI arguments and options
    cli_parser.add_required_option("h", "hostname", "The hostname of this node that other nodes can use", &local_hostname);
    cli_parser.add_option("i", "introducer", "Introducer hostname, which may be omitted if we are the first node", &introducer);
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
    test_parser->add_argument("prefix", "The prefix of the tests to run, where \"all\" will run all tests", &test_prefix);
    test_parser->add_option("l", "log_level", "The logging level to use: either OFF, INFO, DEBUG, or TRACE", log_level_parser);

    // Run CLI parser and exit on failure
    if (!cli_parser.parse("member", argc, argv)) {
        return 1;
    }

    unique_ptr<logger> lg = make_unique<logger>("", log_level);

    if (test_parser->was_invoked()) {
        testing::run_tests((test_prefix == "all" ? "" : test_prefix), lg.get(), true);
    } else {
        unique_ptr<udp_client_intf> udp_client_inst = make_unique<udp_client>(lg.get());
        unique_ptr<udp_server_intf> udp_server_inst = make_unique<udp_server>(lg.get());
        unique_ptr<member_list> mem_list = make_unique<member_list>(local_hostname, lg.get());

        unique_ptr<heartbeater_intf> hb;
        if (introducer == "") {
            hb = make_unique<heartbeater<true>>(mem_list.get(), lg.get(), udp_client_inst.get(), udp_server_inst.get(), local_hostname, port);
        } else {
            hb = make_unique<heartbeater<false>>(mem_list.get(), lg.get(), udp_client_inst.get(), udp_server_inst.get(), local_hostname, port);
        }

        hb->start();

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    return 0;
}
