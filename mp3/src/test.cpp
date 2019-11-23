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
#include <mutex>

#define PARALLELISM 8

using std::unique_ptr;
using std::make_unique;

std::vector<std::tuple<std::string, std::string, std::function<void(logger::log_level)>>> testing::tests;

testing::register_test::register_test(std::string name, std::string description, std::function<void(logger::log_level)> test_fn) {
    tests.push_back(std::make_tuple(name, description, test_fn));
}

void testing::run_tests(std::string prefix, logger::log_level level, bool show_description) {
    std::vector<test*> tests_to_run;

    for (unsigned i = 0; i < tests.size(); i++) {
        std::string test_name = std::get<0>(tests[i]);

        if (test_name.find(prefix) == 0) {
            tests_to_run.push_back(&tests[i]);
        }
    }

    // If there is no logging, run the tests in parallel
    unsigned num_threads = (level == logger::log_level::level_off) ? PARALLELISM : 1;
    std::vector<std::thread> test_threads;
    std::mutex cout_mutex;
    // Iterate through the number of threads and assign tasks to threads in serial
    for (unsigned i = 0; i < num_threads; i++) {
        test_threads.push_back(std::thread([i, &tests_to_run, level, show_description, &cout_mutex, num_threads] {
            // Allocate tests evenly to threads
            for (unsigned j = i; j < tests_to_run.size(); j += num_threads) {
                std::string test_name = std::get<0>(*tests_to_run[j]);
                std::string test_description = std::get<1>(*tests_to_run[j]);

                { // Atomically print out that we are running the test
                    std::lock_guard<std::mutex> guard(cout_mutex);
                    std::cout << "=== Running test " << test_name << " on test thread " << std::to_string(i) << " ===" << std::endl;
                    if (show_description) {
                        std::cout << "=== " << test_description << " ===" << std::endl;
                    }
                }

                std::function<void(logger::log_level)> test_fn = std::get<2>(*tests_to_run[j]);
                test_fn(level);
            }
        }));
    }

    for (unsigned i = 0; i < num_threads; i++) {
        test_threads[i].join();
    }
    std::cout << "=== SUCCESSFULLY PASSED ALL TESTS ===" << std::endl;
}
