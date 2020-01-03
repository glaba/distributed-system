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

    auto parse(std::string const& command, int argc, char **argv) -> bool;

    // Returns true if this command was invoked
    auto was_invoked() const -> bool {
        return invoked;
    }

    // Adds a subcommand and returns a pointer to the subcommand object
    auto add_subcommand(std::string const& command) -> cli_command*;
    // Adds an argument that must be present, which will be stored in the provided pointer
    // If a callback is passed instead of a pointer, the callback will be called
    template <typename T> // T may be: int, string, callback
    void add_argument(std::string const& short_desc, std::string const& description, T *result) {
        arguments.push_back(result);
        argument_descriptions.push_back(description);
        argument_short_descriptions.push_back(short_desc);
    }

    // Adds an option whose result will be stored in the provided pointer
    // If the pointer is a bool, it will just check for the presence of the option, but otherwise an argument must follow
    // If a callback is passed instead of a pointer, the callback will be called
    template <typename T, typename V = T> // T may be: bool, int, string, callback, V should be the same as T except for callback, where it should be 
    void add_option(std::string const& flag, std::string const& short_desc, std::string const& description, T *result, V default_value = V()) {
        options[flag] = result;
        option_descriptions[flag] = description;
        option_short_descriptions[flag] = short_desc;
        option_required[flag] = false;
        option_defaults[flag] = default_value;
    }
    // Adds a required option whose result will be stored in the provided pointer
    // If a callback is passed instead of a pointer, the callback will be called
    template <typename T> // T may be: int, string, callback
    void add_required_option(std::string const& flag, std::string const& short_desc, std::string const& description, T *result) {
        options[flag] = result;
        option_descriptions[flag] = description;
        option_short_descriptions[flag] = short_desc;
        option_required[flag] = true;
        if constexpr (std::is_same<T, callback>::value) {
            option_defaults[flag] = std::monostate();
        } else {
            option_defaults[flag] = T();
        }
    }
private:
    using option_result = std::variant<bool*, std::string*, int*, std::function<bool(std::string)>*>;
    using option_value = std::variant<std::monostate, bool, std::string, int>;
    using argument_result = std::variant<std::string*, int*, std::function<bool(std::string)>*>;

    void print_help(std::string const& command);

    bool invoked = false;
    std::unordered_map<std::string, std::unique_ptr<cli_command>> subcommands;
    std::vector<argument_result> arguments;
    std::vector<std::string> argument_descriptions;
    std::vector<std::string> argument_short_descriptions;
    std::unordered_map<std::string, option_result> options;
    std::unordered_map<std::string, std::string> option_descriptions;
    std::unordered_map<std::string, std::string> option_short_descriptions;
    std::unordered_map<std::string, bool> option_required;
    std::unordered_map<std::string, option_value> option_defaults;
};

extern cli_command cli_parser;
