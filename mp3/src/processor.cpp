#include "processor.h"

#include <iostream>

using std::string;

std::unique_ptr<processor> processor_factory::get_processor(processor::type type, std::string output_dir) {
    switch (type) {
        case processor::type::maple: return std::make_unique<maple_processor>(output_dir);
        case processor::type::juice: return std::make_unique<juice_processor>(output_dir);
        default: assert(false); return nullptr;
    }
}

maple_processor::maple_processor(string output_dir_)
    : inputter([this] () -> std::optional<std::pair<std::string, std::vector<std::string>>> {
        if (kv_pairs.size() == 0) {
            return std::nullopt;
        } else {
            auto it = kv_pairs.begin();
            auto retval = *it;
            kv_pairs.erase(it);
            return retval;
        }
    }), output_dir(output_dir_) {}

bool maple_processor::process_line(string line) {
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

void maple_processor::reset() {
    kv_pairs.clear();
}

juice_processor::juice_processor(string output_file_)
    : inputter([this] () -> std::optional<std::pair<std::string, std::vector<std::string>>> {
        if (values.size() > 0) {
            std::pair<std::string, std::vector<std::string>> retval;
            retval.first = output_file;
            retval.second = values;
            values.clear();
            return retval;
        } else {
            return std::nullopt;
        }
    }), output_file(output_file_) {}

bool juice_processor::process_line(string line) {
    // We assume the juice executable already emits the format <key> <value>
    values.push_back(line);
    return true;
}

void juice_processor::reset() {
    values.clear();
}
