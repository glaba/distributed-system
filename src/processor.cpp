#include "processor.h"

#include <iostream>

using std::string;

std::unique_ptr<processor> processor_factory::get_processor(processor::type type) {
    switch (type) {
        case processor::type::maple: return std::make_unique<maple_processor>();
        case processor::type::juice: return std::make_unique<juice_processor>();
        default: assert(false); return nullptr;
    }
}

maple_processor::maple_processor()
    : inputter([this] () -> std::optional<std::pair<std::string, std::vector<std::string>>> {
        if (kv_pairs.size() == 0) {
            return std::nullopt;
        } else {
            auto it = kv_pairs.begin();
            auto retval = *it;
            kv_pairs.erase(it);
            return retval;
        }
    }) {}

bool maple_processor::process_line(string line) {
    // The input has the format "<key> <value>", where <key> doesn't contain spaces and <value> doesn't contain \n
    size_t space_index = line.find(" ");
    if (space_index == string::npos) {
        return false;
    }
    string key = line.substr(0, space_index);
    string value = line.substr(space_index + 1);
    kv_pairs[key].push_back(value);
    return true;
}

void maple_processor::reset() {
    kv_pairs.clear();
}

juice_processor::juice_processor()
    : inputter([this] () -> std::optional<std::pair<std::string, std::vector<std::string>>> {
        if (values.size() > 0) {
            std::pair<std::string, std::vector<std::string>> retval;
            retval.first = "output";
            retval.second = values;
            values.clear();
            return retval;
        } else {
            return std::nullopt;
        }
    }) {}

bool juice_processor::process_line(string line) {
    // We assume the juice executable already emits the format <key> <value>
    values.push_back(line);
    return true;
}

void juice_processor::reset() {
    values.clear();
}
