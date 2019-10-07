#include "member.h"
#include "heartbeat.h"
#include "utils.h"
#include "logging.h"
#include "member_list.h"
#include "test.h"

bool testing = false;

int process_params(int argc, char **argv, std::string *introducer, std::string *local_hostname,
        uint16_t *port, bool *testing, bool *verbose, bool *is_introducer);

int main(int argc, char **argv) {
    std::string introducer = "";
    bool is_introducer = false;
    std::string local_hostname = "";
    uint16_t port = DEFAULT_PORT;
    bool testing = false;
    bool verbose = false;

    if (process_params(argc, argv, &introducer, &local_hostname, &port, &testing, &verbose, &is_introducer)) {
        return 1;
    }

    logger *lg = new logger("", verbose);

    if (testing) {
        int retval = run_tests(lg);
        // int retval = 0;
        delete lg;
        return retval;
    }

    udp_client_svc *udp_client = new udp_client_svc(lg);
    udp_server_svc *udp_server = new udp_server_svc(lg);

    heartbeater *hb;
    if (introducer == "none") {
        hb = new heartbeater(member_list(local_hostname, lg), lg, udp_client, udp_server, local_hostname, port);
    } else {
        hb = new heartbeater(member_list(local_hostname, lg), lg, udp_client, udp_server, local_hostname, is_introducer, introducer, port);
    }

    hb->start();

    delete lg;
    delete udp_client;
    delete udp_server;
    delete hb;
    return 0;
}

void print_help() {
    std::cout << "Usage: member [test | -i <introducer> -h <hostname> -p <port>] -v" << std::endl << std::endl;
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
        uint16_t *port, bool *testing, bool *verbose, bool *is_introducer) {
    if (argc == 1)
        return print_invalid();

    if (std::string(argv[1]) == "test") {
        *testing = true;

        if (argc >= 3 && std::string(argv[2]) == "-v")
            *verbose = true;

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
                *local_hostname = argv[i + 1];
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
