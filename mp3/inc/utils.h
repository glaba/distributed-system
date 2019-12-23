#pragma once

#include <functional>

namespace utils {
    void backoff(std::function<bool()> callback);
}
