#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cassert>

class outputter {
public:
    enum type {
        maple, juice
    };

    static std::string print_type(type t) {
        switch (t) {
            case maple: return "maple_outputter";
            case juice: return "juice_outputter";
            default: assert(false);
        }
        return "";
    }

    // Processes a single line of the output of the executable
    virtual bool process_line(std::string line) = 0;
    // Resets any internal state accumulated from processing lines
    virtual void reset() = 0;
    // Returns the output file to append to as well as a vector of the values to append to the file
    virtual std::pair<std::string, std::vector<std::string>> emit() = 0;
    // Whether or not there is more data to emit
    virtual bool more() = 0;
};

class outputter_factory {
public:
    static std::unique_ptr<outputter> get_outputter(outputter::type t, std::string output_dir);

private:
    class maple_outputter : public outputter {
    public:
        maple_outputter(std::string output_dir_)
            : output_dir(output_dir_) {}

        bool process_line(std::string line);
        void reset();
        std::pair<std::string, std::vector<std::string>> emit();
        bool more();

    private:
        std::unordered_map<std::string, std::vector<std::string>> kv_pairs;
        std::string output_dir;
    };

    class juice_outputter : public outputter {
    public:
        juice_outputter(std::string output_dir_)
            : output_file(output_dir_) {}

        bool process_line(std::string line);
        void reset();
        std::pair<std::string, std::vector<std::string>> emit();
        bool more();

    private:
        std::vector<std::string> values;
        std::string output_file;
    };
};
