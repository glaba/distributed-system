#include "partitioner.h"

#include <random>
#include <functional>
#include <algorithm>
#include <chrono>

using namespace std::chrono;
using std::unordered_map;
using std::unordered_set;
using std::string;
using std::vector;

auto partitioner_factory::get_partitioner(partitioner::type t) -> std::unique_ptr<partitioner> {
    switch (t) {
        case partitioner::type::round_robin: return std::unique_ptr<partitioner>(new round_robin_partitioner());
        case partitioner::type::hash: return std::unique_ptr<partitioner>(new hash_partitioner());
        case partitioner::type::range: return std::unique_ptr<partitioner>(new range_partitioner());
        default: assert(false);
    }
    return nullptr;
}

auto partitioner_factory::round_robin_partitioner::partition(vector<member> const& members,
    unsigned num_workers, vector<string> const& input_files) const -> unordered_map<string, unordered_set<string>>
{
    // Pick a random subset of the nodes and perform round robin partitioning on them
    vector<member> subset;
    std::sample(members.begin(), members.end(), std::back_inserter(subset), num_workers,
        std::mt19937(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()));

    unordered_map<string, unordered_set<string>> retval;
    for (unsigned i = 0; i < input_files.size(); i++) {
        retval[subset[i % subset.size()].hostname].insert(input_files[i]);
    }

    return retval;
}

auto partitioner_factory::hash_partitioner::partition(vector<member> const& members,
    unsigned num_workers, vector<string> const& input_files) const -> unordered_map<string, unordered_set<string>>
{
    // Pick a random subset of the nodes and perform hash partitioning on them
    vector<member> subset;
    std::sample(members.begin(), members.end(), std::back_inserter(subset), num_workers,
        std::mt19937(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()));

    unordered_map<string, unordered_set<string>> retval;
    for (unsigned i = 0; i < input_files.size(); i++) {
        unsigned index = std::hash<string>()(input_files[i]);
        retval[subset[index].hostname].insert(input_files[i]);
    }

    return retval;
}

auto partitioner_factory::range_partitioner::partition(vector<member> const& members,
    unsigned num_workers, vector<string> const& input_files) const -> unordered_map<string, unordered_set<string>>
{
    unsigned num_tasks = (num_workers < members.size()) ? num_workers : members.size();

    // Pick a random subset of the nodes and perform range partitioning on them
    vector<member> subset;
    std::sample(members.begin(), members.end(), std::back_inserter(subset), num_workers,
        std::mt19937(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()));

    // Sort the input_files in alphabetical order
    std::vector<string> input_files_sorted = input_files;
    std::sort(input_files_sorted.begin(), input_files_sorted.end());

    // Assign input files to the nodes based on ranges
    // If a = floor(num input files / num_tasks), we must assign n nodes a files, and m nodes a+1 files
    unsigned a = input_files_sorted.size() / num_tasks;
    unsigned m = input_files_sorted.size() % num_tasks;
    unsigned n = num_tasks - m;

    unordered_map<string, unordered_set<string>> retval;
    unsigned i = 0;
    // Assign a files to the first n nodes
    for (unsigned j = 0; j < n; j++) {
        for (unsigned k = 0; k < a; k++) {
            retval[subset[j].hostname].insert(input_files_sorted[i]);
            i++;
        }
    }
    // Assign a+1 files to the next m nodes
    for (unsigned j = n; j < m + n; j++) {
        for (unsigned k = 0; k < a + 1; k++) {
            retval[subset[j].hostname].insert(input_files_sorted[i]);
            i++;
        }
    }

    assert(i == input_files_sorted.size());
    assert(retval.size() == num_tasks || input_files_sorted.size() < num_tasks);

    return retval;
}
