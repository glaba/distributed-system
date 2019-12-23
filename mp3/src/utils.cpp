#include "utils.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <string>

namespace utils {
    void backoff(std::function<bool()> callback) {
        unsigned delay = 1;
        while (!callback()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % delay));
            delay *= 2;
            if (delay >= 1000) {
                std::cout << "Waiting in a backoff" << std::endl;
            }
        }
    }
}
