#include "lamport.hpp"

#include <algorithm>

auto lamport_impl::update(uint64_t ts) -> uint64_t {
    unlocked<uint64_t> counter = counter_lock();
    *counter = std::max(*counter, ts) + 1;
    return *counter;
}

auto lamport_impl::increment() -> uint64_t {
    return ++(*counter_lock());
}

auto lamport_impl::get() -> uint64_t {
    return *counter_lock();
}

register_auto<lamport, lamport_impl> register_lamport;
