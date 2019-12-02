#include "cli.h"

#include <unordered_set>
#include <iostream>
#include <functional>

using std::unique_ptr;
using std::make_unique;
using std::string;

cli_command cli_parser;

cli_command *cli_command::add_subcommand(string command) {
    subcommands[command] = make_unique<cli_command>();
    return subcommands[command].get();
}

template <typename T>
void cli_command::add_argument(string short_desc, string description, T *result) {
    arguments.push_back(result);
    argument_descriptions.push_back(description);
    argument_short_descriptions.push_back(short_desc);
}

template <typename T>
void cli_command::add_option(string flag, string short_desc, string description, T *result) {
    options[flag] = result;
    option_descriptions[flag] = description;
    option_short_descriptions[flag] = short_desc;
    option_required[flag] = false;
}

template <typename T>
void cli_command::add_required_option(string flag, string short_desc, string description, T *result) {
    options[flag] = result;
    option_descriptions[flag] = description;
    option_short_descriptions[flag] = short_desc;
    option_required[flag] = true;
}

bool cli_command::parse(string command, int argc, char **argv) {
    { // Block to keep all local variables out of scope of help label
        // Assume that the current command was invoked
        argc--; argv++;
        invoked = true;

        // First, check if the next string is a subcommand
        if (argc > 0) {
            for (auto &subcommand : subcommands) {
                if (subcommand.first == string(argv[0])) {
                    return subcommand.second->parse(command + " " + subcommand.first, argc, argv);
                }
            }
        }

        // If it was not, we are invoking this command
        // Parse the arguments first
        for (unsigned i = 0; i < arguments.size(); i++) {
            if (argc == 0) {
                std::cout << "Missing argument " << argument_short_descriptions[i] << "!" << std::endl << std::endl;
                goto help;
            }

            string cur_arg = string(argv[0]);
            argc--; argv++;

            if (std::holds_alternative<string*>(arguments[i])) {
                *std::get<string*>(arguments[i]) = cur_arg;
            }

            if (std::holds_alternative<int*>(arguments[i])) {
                try {
                    *std::get<int*>(arguments[i]) = std::stoi(cur_arg);
                } catch (...) {
                    std::cout << "Invalid format for argument " << argument_short_descriptions[i] << "!" << std::endl << std::endl;
                    goto help;
                }
            }

            if (std::holds_alternative<std::function<bool(string)>*>(arguments[i])) {
                std::function<bool(string)> callback = *std::get<std::function<bool(string)>*>(arguments[i]);

                if (!callback(cur_arg)) {
                    std::cout << "Invalid format for argument " << argument_short_descriptions[i] << "!" << std::endl << std::endl;
                    goto help;
                }
            }
        }

        // First, set all options to their default empty values
        for (auto option : options) {
            if (std::holds_alternative<bool*>(option.second)) {
                *std::get<bool*>(option.second) = false;
            }

            if (std::holds_alternative<string*>(option.second)) {
                *std::get<string*>(option.second) = "";
            }

            if (std::holds_alternative<int*>(option.second)) {
                *std::get<int*>(option.second) = 0;
            }
        }

        // Then parse the options one by one
        std::unordered_set<string> parsed_options;
        while (argc != 0) {
            string cur_arg = string(argv[0]);
            argc--; argv++;

            // Make sure that it is in fact an option
            if (cur_arg.at(0) != '-') {
                std::cout << "Expected option instead of " << cur_arg << std::endl << std::endl;
                goto help;
            }

            // Check that the option is in our list of options
            string flag = cur_arg.substr(1);
            if (options.find(flag) == options.end()) {
                std::cout << "Unrecognized option " << cur_arg << std::endl << std::endl;
                goto help;
            }

            // Make sure the option isn't repeated
            if (parsed_options.find(flag) != parsed_options.end()) {
                std::cout << "Option " << cur_arg << " passed in twice" << std::endl << std::endl;
                goto help;
            }

            // If it is a boolean option, simply note that it's here
            if (std::holds_alternative<bool*>(options[flag])) {
                *std::get<bool*>(options[flag]) = true;
                parsed_options.insert(flag);
            }

            // We expect an argument for the rest of the types
            if (argc == 0) {
                std::cout << "Expected argument for option " << cur_arg << std::endl << std::endl;
                goto help;
            }

            // If it's a string option, make sure the argument is there and store the result
            if (std::holds_alternative<string*>(options[flag])) {
                *std::get<string*>(options[flag]) = string(argv[0]);
                argc--; argv++;
                parsed_options.insert(flag);
            }

            if (std::holds_alternative<int*>(options[flag])) {
                try {
                    *std::get<int*>(options[flag]) = std::stoi(argv[0]);
                    argc--; argv++;
                    parsed_options.insert(flag);
                } catch (...) {
                    std::cout << "Invalid format for option " << cur_arg << std::endl << std::endl;
                    goto help;
                }
            }

            // If it's a callback option, make sure the argument is there and call the callback
            if (std::holds_alternative<std::function<bool(string)>*>(options[flag])) {
                string arg = string(argv[0]);
                argc--; argv++;

                std::function<bool(string)> callback = *std::get<std::function<bool(string)>*>(options[flag]);

                // If the callback function returns false, warn the user that it's invalid
                if (!callback(arg)) {
                    std::cout << "Invalid format for option " << cur_arg << std::endl << std::endl;
                    goto help;
                } else {
                    parsed_options.insert(flag);
                }
            }
        }

        // Make sure that all required options were satisfied
        for (auto option : option_required) {
            // If the option was required but was not passed in
            if (option.second && parsed_options.find(option.first) == parsed_options.end()) {
                std::cout << "Expected option -" << option.first << " but was not provided" << std::endl << std::endl;
                goto help;
            }
        }

        return true;
    }

help:
    std::cout << "Usage: " << command << " ";

    // Print all the arguments
    for (unsigned i = 0; i < arguments.size(); i++) {
        std::cout << "<" << argument_short_descriptions[i] << "> ";
    }

    // Print all the options
    for (auto option : options) {
        string flag = option.first;
        if (!option_required[flag]) std::cout << "[";
        std::cout << "-" << flag;
        if (!std::holds_alternative<bool*>(option.second)) {
            std::cout << " <" << option_short_descriptions[flag] << ">";
        }
        if (!option_required[flag]) std::cout << "]";
        std::cout << " ";
    }
    std::cout << std::endl;

    if (subcommands.size() > 0) {
        // Print out potential subcommands
        for (auto it = subcommands.begin(); it != subcommands.end(); ++it) {
            std::cout << "       [" << command << " " << it->first << " ...]" << std::endl;
        }
    }

    // Print the descriptions of the arguments and options
    for (unsigned i = 0; i < arguments.size(); i++) {
        std::cout << " <" << argument_short_descriptions[i] << ">\t" << argument_descriptions[i] << std::endl;
    }
    for (auto option : options) {
        std::cout << " -" << option.first << "\t\t" << option_descriptions[option.first] << std::endl;
    }

    return false;
}

template void cli_command::add_argument(string short_desc, string description, int *result);
template void cli_command::add_argument(string short_desc, string description, string *result);
template void cli_command::add_argument(string short_desc, string description, std::function<bool(string)> *result);

template void cli_command::add_option(string flag, string short_desc, string description, bool *result);
template void cli_command::add_option(string flag, string short_desc, string description, int *result);
template void cli_command::add_option(string flag, string short_desc, string description, string *result);
template void cli_command::add_option(string flag, string short_desc, string description, std::function<bool(string)> *result);

template void cli_command::add_required_option(string flag, string short_desc, string description, int *result);
template void cli_command::add_required_option(string flag, string short_desc, string description, string *result);
template void cli_command::add_required_option(string flag, string short_desc, string description, std::function<bool(string)> *result);
