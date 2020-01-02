#include "utils.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <string>

namespace utils {
    bool backoff(std::function<bool()> callback, std::function<bool()> give_up) {
        unsigned delay = 1;
        while (!callback()) {
            if (give_up()) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % delay));
            delay *= 2;
        }
        return true;
    }
}
