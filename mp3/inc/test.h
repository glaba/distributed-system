#pragma once

#include "udp.h"
#include "logging.h"

#include <vector>
#include <tuple>
#include <string>
#include <cassert>
#include <functional>

class testing {
public:
    // Runs all tests with the given prefix with logging at the specified level
    static void run_tests(std::string prefix, logger::log_level level, bool show_description);

    class register_test {
    public:
        register_test(std::string name, std::string description, std::function<void(logger::log_level)> test_fn);
    };
private:
    static std::vector<std::tuple<std::string, std::string, std::function<void(logger::log_level)>>> tests;
};
