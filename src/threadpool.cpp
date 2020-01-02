#include "threadpool.h"
#include "threadpool.hpp"

#include <iostream>

std::unique_ptr<threadpool> threadpool_factory_impl::get_threadpool(unsigned num_threads) {
    return std::unique_ptr<threadpool>(new threadpool_impl(env, num_threads));
}

threadpool_impl::threadpool_impl(environment &env, unsigned num_threads_)
    : num_threads(num_threads_)
    , lg(env.get<logger_factory>()->get_logger("threadpool"))
{
    running = true;
    num_started = 0;
    for (unsigned i = 0; i < num_threads; i++) {
        threads.push_back(std::thread([this, i] {
            thread_fn(i);
        }));
    }
    while (num_started.load() < num_threads) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

threadpool_impl::~threadpool_impl() {
    {
        std::unique_lock<std::mutex> lock(cv_mutex);
        if (!running.load()) {
            return;
        }
    }
    finish();
}

void threadpool_impl::finish() {
    {
        std::unique_lock<std::mutex> lock(cv_mutex);
        cv_finished.wait(lock, [this] {
            return tasks.empty() && working == 0;
        });
        running = false;
    }
    cv_task.notify_all();
    for (unsigned i = 0; i < num_threads; i++) {
        threads[i].join();
    }
}

void threadpool_impl::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> guard(cv_mutex);
        tasks.push(task);
    }
    cv_task.notify_one();
}

void threadpool_impl::thread_fn(unsigned thread_index) {
    num_started++;
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(cv_mutex);
            cv_task.wait(lock, [this] {
                return !tasks.empty() || !running.load();
            });

            if (!running.load()) {
                break;
            }

            task = tasks.front();
            tasks.pop();
            working++;
        }

        task();
        {
            std::lock_guard<std::mutex> guard(cv_mutex);
            working--;
        }
        cv_finished.notify_all();
    }
}

register_auto<threadpool_factory, threadpool_factory_impl> register_threadpool_factory;
