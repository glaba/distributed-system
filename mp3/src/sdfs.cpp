#include "sdfs.h"

int main(int argc, char **argv) {
    std::cout << "hi" << std::endl;
}

/*
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
                *local_hostname = std::string(argv[i + 1]);
            }
            i++;
        // Whether or not to use verbose logging
        } else if (std::string(argv[i]) == "-v") {
            *verbose = true;
        } else if (std::string(argv[i]) == "-n") {
            *is_introducer = true;
        } else return print_invalid();
*/
