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
#include <queue>
#include <mutex>
#include <climits>

#define PARALLELISM 8

using std::unique_ptr;
using std::make_unique;

std::vector<std::tuple<std::string, std::string, unsigned, std::function<void(logger::log_level)>>> testing::tests;

testing::register_test::register_test(std::string name, std::string description, unsigned approx_length, std::function<void(logger::log_level)> test_fn) {
    tests.push_back(std::make_tuple(name, description, approx_length, test_fn));
}

void testing::run_tests(std::string prefix, logger::log_level level, bool show_description) {
    std::vector<test*> tests_to_run;

    for (unsigned i = 0; i < tests.size(); i++) {
        std::string test_name = std::get<0>(tests[i]);

        if (test_name.find(prefix) == 0) {
            tests_to_run.push_back(&tests[i]);
        }
    }

    // Sort the tests by the time they take to run so that we assign the longest running tests first
    std::sort(tests_to_run.begin(), tests_to_run.end(), [] (test *t1, test *t2) {
        return std::get<2>(*t1) > std::get<2>(*t2);
    });

    // If there is no logging, run the tests in parallel
    unsigned num_threads = (level == logger::log_level::level_off) ? PARALLELISM : 1;

    // Fairly distribute the workload between threads
    std::vector<unsigned> thread_workloads;
    std::vector<std::vector<unsigned>> tests_per_thread;
    for (unsigned i = 0; i < num_threads; i++) {
        thread_workloads.push_back(0);
        tests_per_thread.push_back(std::vector<unsigned>());
    }
    for (unsigned i = 0; i < tests_to_run.size(); i++) {
        // Find the thread with the lowest workload so far
        unsigned min = UINT_MAX;
        unsigned min_index = 0;
        for (unsigned j = 0; j < num_threads; j++) {
            if (thread_workloads[j] < min) {
                min = thread_workloads[j];
                min_index = j;
            }
        }

        // Assign the current test to the least loaded thread
        tests_per_thread[min_index].push_back(i);
        thread_workloads[min_index] += std::get<2>(*tests_to_run[i]);
    }

    std::vector<std::thread> test_threads;
    std::mutex cout_mutex;
    // Iterate through the number of threads and run tasks in each thread in serial
    for (unsigned i = 0; i < num_threads; i++) {
        test_threads.push_back(std::thread([i, &tests_to_run, level, show_description, &cout_mutex, num_threads, &tests_per_thread] {
            // Run the tasks that were assigned to this thread
            for (unsigned test_index : tests_per_thread[i]) {
                std::string test_name = std::get<0>(*tests_to_run[test_index]);
                std::string test_description = std::get<1>(*tests_to_run[test_index]);

                { // Atomically print out that we are running the test
                    std::lock_guard<std::mutex> guard(cout_mutex);
                    std::cout << "=== Running test " << test_name << " on test thread " << std::to_string(i) << " ===" << std::endl;
                    if (show_description) {
                        std::cout << "=== " << test_description << " ===" << std::endl;
                    }
                }

                std::function<void(logger::log_level)> test_fn = std::get<3>(*tests_to_run[test_index]);
                test_fn(level);
            }
        }));
    }

    for (unsigned i = 0; i < num_threads; i++) {
        test_threads[i].join();
    }
    std::cout << "=== SUCCESSFULLY PASSED ALL TESTS ===" << std::endl;
}
