#pragma once

#include "lamport.h"
#include "environment.h"
#include "service.h"
#include "locking.h"

class lamport_impl : public lamport, public service_impl<lamport> {
public:
    lamport_impl(environment &env) {}

    auto update(uint64_t ts) -> uint64_t;
    auto increment() -> uint64_t;
    auto get() -> uint64_t;

private:
    locked<uint64_t> counter_lock;
};
