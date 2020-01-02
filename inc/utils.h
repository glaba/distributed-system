#pragma once

#include <functional>

namespace utils {
    bool backoff(std::function<bool()> callback, std::function<bool()> give_up = [] {return false;});
}
