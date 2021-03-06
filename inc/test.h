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
    static void run_tests(std::string const& prefix, logger::log_level level, int parallelism, bool show_description);

    class register_test {
    public:
        register_test(std::string const& name, std::string const& description,
            unsigned approx_length, std::function<void(logger::log_level)> const& test_fn);
    };
private:
    using test = std::tuple<std::string, std::string, unsigned, std::function<void(logger::log_level)>>;
    static std::vector<test> tests;
};
