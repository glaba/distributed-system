#pragma once

#include <functional>
#include <memory>

class threadpool {
public:
    virtual ~threadpool() {}
    virtual void enqueue(std::function<void()> task) = 0;
    virtual void finish() = 0;
};

class threadpool_factory {
public:
    virtual std::unique_ptr<threadpool> get_threadpool(unsigned num_threads) = 0;
};

