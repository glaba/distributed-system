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

    static auto print_type(type t) -> std::string {
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
    virtual auto partition(std::vector<member> const& members, unsigned num_workers, std::vector<std::string> const& input_files)
        const -> std::unordered_map<std::string, std::unordered_set<std::string>> = 0;
};

class partitioner_factory {
public:
    static auto get_partitioner(partitioner::type t) -> std::unique_ptr<partitioner>;

private:
    class round_robin_partitioner : public partitioner {
    public:
        auto partition(std::vector<member> const& members, unsigned num_workers, std::vector<std::string> const& input_files)
            const -> std::unordered_map<std::string, std::unordered_set<std::string>>;
    };

    class hash_partitioner : public partitioner {
    public:
        auto partition(std::vector<member> const& members, unsigned num_workers, std::vector<std::string> const& input_files)
            const -> std::unordered_map<std::string, std::unordered_set<std::string>>;
    };

    class range_partitioner : public partitioner {
    public:
        auto partition(std::vector<member> const& members, unsigned num_workers, std::vector<std::string> const& input_files)
            const -> std::unordered_map<std::string, std::unordered_set<std::string>>;
    };
};
