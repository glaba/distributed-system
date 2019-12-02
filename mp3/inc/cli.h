#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <variant>

class cli_command {
public:
    using callback = std::function<bool(std::string)>;

    cli_command() {}

    bool parse(std::string command, int argc, char **argv);

    // Returns true if this command was invoked
    bool was_invoked() {
        return invoked;
    }

    // Adds a subcommand and returns a pointer to the subcommand object
    cli_command *add_subcommand(std::string command);
    // Adds an argument that must be present, which will be stored in the provided pointer
    // If a callback is passed instead of a pointer, the callback will be called
    template <typename T> // T may be: int, string, callback
    void add_argument(std::string short_desc, std::string description, T *result);

    // Adds an option whose result will be stored in the provided pointer
    // If the pointer is a bool, it will just check for the presence of the option, but otherwise an argument must follow
    // If a callback is passed instead of a pointer, the callback will be called
    template <typename T> // T may be: bool, int, string, callback
    void add_option(std::string flag, std::string short_desc, std::string description, T *result);
    // Adds a required option whose result will be stored in the provided pointer
    // If a callback is passed instead of a pointer, the callback will be called
    template <typename T> // T may be: int, string, callback
    void add_required_option(std::string flag, std::string short_desc, std::string description, T *result);
private:
    using option_result = std::variant<bool*, std::string*, int*, std::function<bool(std::string)>*>;
    using argument_result = std::variant<std::string*, int*, std::function<bool(std::string)>*>;

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
