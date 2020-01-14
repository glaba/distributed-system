#include "utils.h"
#include "stacktrace.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <cassert>

namespace utils {
    bool backoff(std::function<bool()> const& callback, std::function<bool()> const& give_up) {

        unsigned delay = 1;
        while (!callback()) {
            if (give_up()) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % delay));
            delay *= 2;
            if (delay > 5000) {
                print_stacktrace();
                assert(false && "Waited too long in backoff");
            }
        }
        return true;
    }
}
