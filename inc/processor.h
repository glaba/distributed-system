#pragma once

#include "inputter.h"

#include <string>
#include <memory>
#include <cassert>

class processor : public virtual inputter<std::pair<std::string, std::vector<std::string>>> {
public:
    enum type {
        maple, juice
    };

    static auto print_type(type t) -> std::string {
        switch (t) {
            case maple: return "maple_processor";
            case juice: return "juice_processor";
            default: assert(false); return "";
        }
    }

    // Processes a single line of the output of the executable
    virtual auto process_line(std::string const& line) -> bool = 0;
    // Resets any internal state accumulated from processing lines
    virtual void reset() = 0;

    // Inputter will provided key-value pairs where the key is the name of the output file
    // and the value is a vector of values corresponding to the key
    // The same key should never be emitted more than once!
};

class processor_factory {
public:
    static auto get_processor(processor::type type) -> std::unique_ptr<processor>;
};

class maple_processor : public processor {
public:
    maple_processor();
    auto process_line(std::string const& line) -> bool;
    void reset();

private:
    std::unordered_map<std::string, std::vector<std::string>> kv_pairs;
};

class juice_processor : public processor {
public:
    juice_processor();
    auto process_line(std::string const& line) -> bool;
    void reset();

private:
    std::vector<std::string> values;
};
