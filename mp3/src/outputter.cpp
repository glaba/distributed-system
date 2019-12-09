#include "outputter.h"

#include <cassert>

using std::string;

std::unique_ptr<outputter> outputter_factory::get_outputter(outputter::type t, string output_dir) {
    switch (t) {
        case outputter::type::maple: return std::unique_ptr<outputter>(new maple_outputter(output_dir));
        case outputter::type::juice: return std::unique_ptr<outputter>(new juice_outputter(output_dir));
        default: assert(false);
    }
    return nullptr;
}

bool outputter_factory::maple_outputter::process_line(string line) {
    // The input has the format "<key> <value>", where <key> doesn't contain spaces and <value> doesn't contain \n
    size_t space_index = line.find(" ");
    if (space_index == string::npos) {
        return false;
    }
    string key = line.substr(0, space_index);
    string value = line.substr(space_index + 1);
    kv_pairs[output_dir + key].push_back(value);
    return true;
}

void outputter_factory::maple_outputter::reset() {
    kv_pairs.clear();
}

std::pair<std::string, std::vector<std::string>> outputter_factory::maple_outputter::emit() {
    auto it = kv_pairs.begin();
    auto retval = *it;
    kv_pairs.erase(it);
    return retval;
}

bool outputter_factory::maple_outputter::more() {
    return kv_pairs.size() > 0;
}

bool outputter_factory::juice_outputter::process_line(string line) {
    // We assume the juice executable already emits the format <key> <value>
    values.push_back(line);
    return true;
}

void outputter_factory::juice_outputter::reset() {
    values.clear();
}

std::pair<std::string, std::vector<std::string>> outputter_factory::juice_outputter::emit() {
    std::pair<std::string, std::vector<std::string>> retval;
    retval.first = output_file;
    retval.second = values;
    values.clear();
    return retval;
}

bool outputter_factory::juice_outputter::more() {
    return values.size() > 0;
}