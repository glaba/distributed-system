#include "test.h"
#include "member_list.h"
#include "mock_udp.h"
#include "heartbeater.h"
#include "election.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

using std::unique_ptr;
using std::make_unique;

std::vector<std::tuple<std::string, std::string, std::function<void(logger::log_level)>>> testing::tests;

testing::register_test::register_test(std::string name, std::string description, std::function<void(logger::log_level)> test_fn) {
    tests.push_back(std::make_tuple(name, description, test_fn));
}

void testing::run_tests(std::string prefix, logger::log_level level, bool show_description) {
    for (unsigned i = 0; i < tests.size(); i++) {
        std::string test_name = std::get<0>(tests[i]);
        std::string test_description = std::get<1>(tests[i]);

        if (test_name.find(prefix) == 0) {
            std::function<void(logger::log_level)> test_fn = std::get<2>(tests[i]);
            std::cout << "=== Running test " << test_name << " ===" << std::endl;
            if (show_description) {
                std::cout << "=== " << test_description << " ===" << std::endl;
            }
            test_fn(level);
        }
    }
    std::cout << "=== SUCCESSFULLY PASSED ALL TESTS ===" << std::endl;
}
