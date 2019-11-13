#pragma once

#include "logging.h"
#include "udp.h"

#include <vector>
#include <tuple>
#include <string>
#include <cassert>
#include <functional>

class testing {
public:
    // Runs all tests with the given prefix
    static void run_tests(std::string prefix, logger *lg, bool show_description);

    class register_test {
    public:
        register_test(std::string name, std::string description, std::function<void(logger*)> test_fn);
    };
private:
    static std::vector<std::tuple<std::string, std::string, std::function<void(logger*)>>> tests;
};
