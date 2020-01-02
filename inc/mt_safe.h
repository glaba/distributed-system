#pragma once

#include <random>
#include <mutex>

// A threadsafe wrapper around std::mt19937
class mt_safe {
public:
    mt_safe(uint32_t seed) : mt(seed) {}

    uint32_t operator()() {
        std::lock_guard<std::recursive_mutex> guard(m);
        return mt();
    }
private:
    std::recursive_mutex m;
    std::mt19937 mt;
};
