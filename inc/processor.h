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

    static std::string print_type(type t) {
        switch (t) {
            case maple: return "maple_processor";
            case juice: return "juice_processor";
            default: assert(false); return "";
        }
    }

    // Processes a single line of the output of the executable
    virtual bool process_line(std::string line) = 0;
    // Resets any internal state accumulated from processing lines
    virtual void reset() = 0;
};

class processor_factory {
public:
    static std::unique_ptr<processor> get_processor(processor::type type, std::string output_dir);
};

class maple_processor : public processor {
public:
    maple_processor(std::string output_dir_);

    bool process_line(std::string line);
    void reset();

private:
    std::unordered_map<std::string, std::vector<std::string>> kv_pairs;
    std::string output_dir;
};

class juice_processor : public processor {
public:
    juice_processor(std::string output_dir_);

    bool process_line(std::string line);
    void reset();

private:
    std::vector<std::string> values;
    std::string output_file;
};
