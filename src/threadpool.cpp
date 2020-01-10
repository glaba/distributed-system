#include "threadpool.h"
#include "threadpool.hpp"

#include <iostream>

auto threadpool_factory_impl::get_threadpool(unsigned num_threads) const -> std::unique_ptr<threadpool> {
    return std::unique_ptr<threadpool>(new threadpool_impl(env, num_threads));
}

threadpool_impl::threadpool_impl(environment &env, unsigned num_threads_)
    : num_threads(num_threads_)
    , lg(env.get<logger_factory>()->get_logger("threadpool"))
{
    unlocked<threadpool_state> tp_state = tp_state_lock();

    tp_state->running = true;
    tp_state->num_started = 0;
    tp_state->working = 0;
    for (unsigned i = 0; i < num_threads; i++) {
        threads.push_back(std::thread([&, i] {
            thread_fn(i);
        }));
    }

    cv_started.wait(tp_state.unsafe_get_mutex(), [&] {
        return tp_state->num_started == num_threads;
    });
}

threadpool_impl::~threadpool_impl() {
    if (tp_state_lock()->running) {
        finish();
    }
}

void threadpool_impl::finish() {
    {
        unlocked<threadpool_state> tp_state = tp_state_lock();
        cv_finished.wait(tp_state.unsafe_get_mutex(), [&] {
            return tp_state->tasks.empty() && tp_state->working == 0;
        });
        tp_state->running = false;
    }
    cv_task.notify_all();
    for (unsigned i = 0; i < num_threads; i++) {
        threads[i].join();
    }
}

void threadpool_impl::enqueue(std::function<void()> const& task) {
    tp_state_lock()->tasks.push(task);
    cv_task.notify_one();
}

void threadpool_impl::thread_fn(unsigned thread_index) {
    tp_state_lock()->num_started++;
    cv_started.notify_one();

    while (true) {
        std::function<void()> task;
        {
            unlocked<threadpool_state> tp_state = tp_state_lock();
            cv_task.wait(tp_state.unsafe_get_mutex(), [&] {
                return !tp_state->tasks.empty() || !tp_state->running;
            });

            if (!tp_state->running) {
                break;
            }

            task = tp_state->tasks.front();
            tp_state->tasks.pop();
            tp_state->working++;
        }

        task();
        tp_state_lock()->working--;
        cv_finished.notify_all();
    }
}

register_auto<threadpool_factory, threadpool_factory_impl> register_threadpool_factory;
