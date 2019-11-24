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
    // Tests will be run in parallel if there is no logging
    static void run_tests(std::string prefix, logger::log_level level, bool show_description);

    class register_test {
    public:
        register_test(std::string name, std::string description, unsigned approx_length, std::function<void(logger::log_level)> test_fn);
    };
private:
    using test = std::tuple<std::string, std::string, unsigned, std::function<void(logger::log_level)>>;
    static std::vector<test> tests;
};
