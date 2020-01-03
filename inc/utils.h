#pragma once

#include <functional>

namespace utils {
    bool backoff(std::function<bool()> const& callback, std::function<bool()> const& give_up = [] {return false;});
}
