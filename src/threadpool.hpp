#pragma once

#include "environment.h"
#include "logging.h"
#include "threadpool.h"

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <memory>

class threadpool_impl : public threadpool {
public:
    threadpool_impl(environment &env, unsigned num_threads_);
    ~threadpool_impl();

    // Must be called before destructor and completely after any calls to enqueue
    void finish();
    void enqueue(std::function<void()> const& task);

private:
    void thread_fn(unsigned thread_index);

    unsigned num_threads;
    std::vector<std::thread> threads;

    struct threadpool_state {
        bool running;
        unsigned num_started;
        std::queue<std::function<void()>> tasks;
        unsigned working;
    };
    locked<threadpool_state> tp_state_lock;
    std::condition_variable_any cv_started, cv_task, cv_finished;

    std::unique_ptr<logger> lg;
};

class threadpool_factory_impl : public threadpool_factory, public service_impl<threadpool_factory_impl> {
public:
    threadpool_factory_impl(environment &env_) : env(env_) {}

    auto get_threadpool(unsigned num_threads) const -> std::unique_ptr<threadpool>;

private:
    environment &env;
};

