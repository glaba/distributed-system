#pragma once

#include "member_list.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class partitioner {
public:
    virtual ~partitioner() {}

    enum type {
        round_robin, hash, range
    };

    static std::string print_type(type t) {
        switch (t) {
            case round_robin: return "round_robin";
            case hash: return "hash";
            case range: return "range";
            default: assert(false);
        }
        return "";
    }

    // Returns a map of hostnames to a set of files assigned to each host, given the load of each host,
    //  the number of desired workers, and the list of input files
    virtual std::unordered_map<std::string, std::unordered_set<std::string>> partition(std::vector<member> members,
        unsigned num_workers, std::vector<std::string> input_files) = 0;
};

class partitioner_factory {
public:
    static std::unique_ptr<partitioner> get_partitioner(partitioner::type t);

private:
    class round_robin_partitioner : public partitioner {
    public:
        std::unordered_map<std::string, std::unordered_set<std::string>> partition(std::vector<member> members,
            unsigned num_workers, std::vector<std::string> input_files);
    };

    class hash_partitioner : public partitioner {
    public:
        std::unordered_map<std::string, std::unordered_set<std::string>> partition(std::vector<member> members,
            unsigned num_workers, std::vector<std::string> input_files);
    };

    class range_partitioner : public partitioner {
    public:
        std::unordered_map<std::string, std::unordered_set<std::string>> partition(std::vector<member> members,
            unsigned num_workers, std::vector<std::string> input_files);
    };
};
