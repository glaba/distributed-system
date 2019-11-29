#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <variant>

class cli_command {
public:
    cli_command() {}

    bool parse(std::string command, int argc, char **argv);

    // Returns true if this command was invoked
    bool was_invoked() {
        return invoked;
    }

    // Adds a subcommand and returns a pointer to the subcommand object
    cli_command *add_subcommand(std::string command);
    // Adds an argument that must be present, which will be stored in the provided pointer
    void add_argument(std::string short_desc, std::string description, std::string *result);
    // Adds an argument that must be present, which will be passed into the callback, which returns false for an invalid arg
    void add_argument(std::string short_desc, std::string description, std::function<bool(std::string)> callback);

    // Adds an option that either is present or not present, and whether or not it is will be stored in the provided pointer
    void add_option(std::string flag, std::string description, bool *result);
    // Adds an option that requires an argument to follow, and the argument will be stored in the provided pointer
    void add_option(std::string flag, std::string short_desc, std::string description, std::string *result);
    // Adds an option that requires an argument to follow, which will be passed into the callback, which returns false if invalid
    void add_option(std::string flag, std::string short_desc, std::string description, std::function<bool(std::string)> callback);
    // Adds a required option that requires an argument to follow, and the argument will be stored in the provided pointer
    void add_required_option(std::string flag, std::string short_desc, std::string description, std::string *result);
    // Adds a required option that requires an argument to follow, which will be passed into the callback, which returns false if invalid
    void add_required_option(std::string flag, std::string short_desc, std::string description, std::function<bool(std::string)> callback);
private:
    using option_result = std::variant<bool*, std::string*, std::function<bool(std::string)>>;
    using argument_result = std::variant<std::string*, std::function<bool(std::string)>>;

    bool invoked = false;
    std::unordered_map<std::string, std::unique_ptr<cli_command>> subcommands;
    std::vector<argument_result> arguments;
    std::vector<std::string> argument_descriptions;
    std::vector<std::string> argument_short_descriptions;
    std::unordered_map<std::string, option_result> options;
    std::unordered_map<std::string, std::string> option_descriptions;
    std::unordered_map<std::string, std::string> option_short_descriptions;
    std::unordered_map<std::string, bool> option_required;
};

extern cli_command cli_parser;
