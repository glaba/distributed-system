#include "member.h"
#include "heartbeater.h"
#include "udp.h"
#include "logging.h"
#include "member_list.h"
#include "test.h"

#include <chrono>
#include <thread>
#include <memory>

using std::unique_ptr;
using std::make_unique;

int process_params(int argc, char **argv, std::string *introducer, std::string *local_hostname,
        uint16_t *port, bool *running_tests, std::string *test_prefix, bool *verbose, bool *is_introducer);

int main(int argc, char **argv) {
    std::string introducer = "";
    bool is_introducer = false;
    std::string local_hostname = "";
    uint16_t port = DEFAULT_PORT;
    bool running_tests = false;
    std::string test_prefix;
    bool verbose = false;

    if (process_params(argc, argv, &introducer, &local_hostname, &port, &running_tests, &test_prefix, &verbose, &is_introducer)) {
        return 1;
    }

    unique_ptr<logger> lg = make_unique<logger>("", verbose);

    if (running_tests) {
        testing::run_tests((test_prefix == "all" ? "" : test_prefix), lg.get(), true);
        return 0;
    }

    unique_ptr<udp_client_intf> udp_client_inst = make_unique<udp_client>(lg.get());
    unique_ptr<udp_server_intf> udp_server_inst = make_unique<udp_server>(lg.get());
    unique_ptr<member_list> mem_list = make_unique<member_list>(local_hostname, lg.get());

    unique_ptr<heartbeater_intf> hb;
    if (introducer == "none") {
        hb = make_unique<heartbeater<true>>(mem_list.get(), lg.get(), udp_client_inst.get(), udp_server_inst.get(), local_hostname, port);
    } else {
        hb = make_unique<heartbeater<false>>(mem_list.get(), lg.get(), udp_client_inst.get(), udp_server_inst.get(), local_hostname, port);
    }

    hb->start();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}

void print_help() {
    std::cout << "Usage: member [test <prefix> | -i <introducer> -h <hostname> -p <port>] -v" << std::endl << std::endl;
    std::cout << "Option\tMeaning" << std::endl;
    std::cout << " -h\tThe hostname of this machine that other members can use" << std::endl;
    std::cout << " -i\tIntroducer hostname or \"none\" if this is the first introducer" << std::endl;
    std::cout << " -n\tThis machine is an introducer" << std::endl;
    std::cout << " -p\tThe port to use" << std::endl;
    std::cout << " -v\tEnable verbose logging" << std::endl;
}

int print_invalid() {
    std::cout << "Invalid arguments" << std::endl;
    print_help();
    return 1;
}

int process_params(int argc, char **argv, std::string *introducer, std::string *local_hostname,
        uint16_t *port, bool *running_tests, std::string *test_prefix, bool *verbose, bool *is_introducer) {
    if (argc == 1)
        return print_invalid();

    if (std::string(argv[1]) == "test") {
        if (argc >= 3) {
            *running_tests = true;
            *test_prefix = std::string(argv[2]);
            if (argc >= 4 && std::string(argv[3]) == "-v")
                *verbose = true;
        } else {
            std::cout << "Must specify prefix of tests to run. A prefix of \"all\" will run all tests" << std::endl;
            return print_invalid();
        }

        return 0;
    }

    for (int i = 1; i < argc; i++) {
        // The introducer
        if (std::string(argv[i]) == "-i") {
            if (i + 1 < argc) {
                *introducer = std::string(argv[i + 1]);
            } else return print_invalid();
            i++;
        // The port to use
        } else if (std::string(argv[i]) == "-p") {
            // Parse the port number from the next argument and fail if it's bad
            if (i + 1 < argc) {
                try {
                    *port = std::stoi(argv[i + 1]);
                } catch (...) {
                    std::cout << "Invalid port number" << std::endl;
                    return print_invalid();
                }
            } else return print_invalid();
            i++;
        // The hostname
        } else if (std::string(argv[i]) == "-h") {
            if (i + 1 < argc) {
                *local_hostname = std::string(argv[i + 1]);
            }
            i++;
        // Whether or not to use verbose logging
        } else if (std::string(argv[i]) == "-v") {
            *verbose = true;
        } else if (std::string(argv[i]) == "-n") {
            *is_introducer = true;
        } else return print_invalid();
    }

    if (*introducer == "") {
        std::cout << "Option -i required" << std::endl;
        return 1;
    }

    if (*local_hostname == "") {
        std::cout << "Option -h required" << std::endl;
        return 1;
    }

    return 0;
}
