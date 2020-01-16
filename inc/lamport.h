#pragma once

#include <cstdint>

class lamport {
public:
    virtual auto update(uint64_t ts) -> uint64_t = 0;
    virtual auto increment() -> uint64_t = 0;
    virtual auto get() -> uint64_t = 0;
};