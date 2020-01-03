#pragma once

#include <functional>
#include <memory>

class threadpool {
public:
    virtual ~threadpool() {}
    virtual void enqueue(std::function<void()> const& task) = 0;
    virtual void finish() = 0;
};

class threadpool_factory {
public:
    virtual auto get_threadpool(unsigned num_threads) const -> std::unique_ptr<threadpool> = 0;
};

