#include "partitioner.h"

#include <random>
#include <functional>
#include <algorithm>

using std::unordered_map;
using std::unordered_set;
using std::string;
using std::vector;

std::unique_ptr<partitioner> partitioner_factory::get_partitioner(partitioner::type t) {
    switch (t) {
        case partitioner::type::round_robin: return std::unique_ptr<partitioner>(new round_robin_partitioner());
        case partitioner::type::hash: return std::unique_ptr<partitioner>(new hash_partitioner());
        case partitioner::type::range: return std::unique_ptr<partitioner>(new range_partitioner());
        default: assert(false);
    }
    return nullptr;
}

unordered_map<string, unordered_set<string>>
partitioner_factory::round_robin_partitioner::partition(vector<member> members,
    unsigned num_workers, vector<string> input_files)
{
    // Pick a random subset of the nodes and perform round robin partitioning on them
    vector<string> subset;
    for (unsigned i = 0; i < num_workers && members.size() > 0; i++) {
        unsigned index = std::rand() % members.size();
        subset.push_back(members[index].hostname);
        members.erase(members.begin() + index);
    }

    unordered_map<string, unordered_set<string>> retval;
    for (unsigned i = 0; i < input_files.size(); i++) {
        retval[subset[i % subset.size()]].insert(input_files[i]);
    }

    return retval;
}

unordered_map<string, unordered_set<string>>
partitioner_factory::hash_partitioner::partition(vector<member> members,
    unsigned num_workers, vector<string> input_files)
{
    // Pick a random subset of the nodes and perform hash partitioning on them
    vector<string> subset;
    for (unsigned i = 0; i < num_workers && members.size() > 0; i++) {
        unsigned index = std::rand() % members.size();
        subset.push_back(members[index].hostname);
        members.erase(members.begin() + index);
    }

    unordered_map<string, unordered_set<string>> retval;
    for (unsigned i = 0; i < input_files.size(); i++) {
        unsigned index = std::hash<string>()(input_files[i]);
        retval[subset[index]].insert(input_files[i]);
    }

    return retval;
}

unordered_map<string, unordered_set<string>>
partitioner_factory::range_partitioner::partition(vector<member> members,
    unsigned num_workers, vector<string> input_files)
{
    unsigned num_tasks = (num_workers < members.size()) ? num_workers : members.size();

    // Pick a random subset of the nodes and perform range partitioning on them
    vector<string> subset;
    for (unsigned i = 0; i < num_workers && members.size() > 0; i++) {
        unsigned index = std::rand() % members.size();
        subset.push_back(members[index].hostname);
        members.erase(members.begin() + index);
    }

    // Sort the input_files in alphabetical order
    std::sort(input_files.begin(), input_files.end());

    // Assign input files to the nodes based on ranges
    // If a = floor(num input files / num_tasks), we must assign n nodes a files, and m nodes a+1 files
    unsigned a = input_files.size() / num_tasks;
    unsigned m = input_files.size() % num_tasks;
    unsigned n = num_tasks - m;

    unordered_map<string, unordered_set<string>> retval;
    unsigned i = 0;
    // Assign a files to the first n nodes
    for (unsigned j = 0; j < n; j++) {
        for (unsigned k = 0; k < a; k++) {
            retval[subset[j]].insert(input_files[i]);
            i++;
        }
    }
    // Assign a+1 files to the next m nodes
    for (unsigned j = n; j < m + n; j++) {
        for (unsigned k = 0; k < a + 1; k++) {
            retval[subset[j]].insert(input_files[i]);
            i++;
        }
    }

    assert(i == input_files.size());
    assert(retval.size() == num_tasks || input_files.size() < num_tasks);

    return retval;
}
