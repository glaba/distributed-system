#include "member.h"
#include "heartbeat.h"
#include "utils.h"
#include "logging.h"
#include "member_list.h"
#include "test.h"

bool testing = false;

int process_params(int argc, char **argv, std::string *introducer, std::string *local_hostname, uint16_t *port, bool *testing);

int main(int argc, char **argv) {
    std::string introducer = "";
    std::string local_hostname = "";
    uint16_t port = DEFAULT_PORT;
    bool testing = false;

    init_logging(DEFAULT_LOGFILE);

    if (process_params(argc, argv, &introducer, &local_hostname, &port, &testing)) {
        return 1;
    }

    if (testing) {
        return run_tests();
    }

    heartbeater *hb;
    if (introducer == "self") {
        hb = new heartbeater(member_list(local_hostname), local_hostname, port);
    } else {
        hb = new heartbeater(member_list(local_hostname), local_hostname, introducer, port);
    }

    hb->start();
    
    delete hb;
    return 0;
}

void print_help() {
    std::cout << "Usage: member [test | -i <introducer> -h <hostname> -p <port>]" << std::endl << std::endl;
    std::cout << "Option\tMeaning" << std::endl;
    std::cout << " -h\tThe hostname of this machine that other members can use" << std::endl;
    std::cout << " -i\tIntroducer hostname or self if this is the introducer" << std::endl;
    std::cout << " -p\tThe port to use" << std::endl;
}

int print_invalid() {
    std::cout << "Invalid arguments" << std::endl;
    print_help();
    return 1;
}

int process_params(int argc, char **argv, std::string *introducer, std::string *local_hostname, uint16_t *port, bool *testing) {
    if (argc == 1)
        return print_invalid();

    if (std::string(argv[1]) == "test") {
        *testing = true;
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
