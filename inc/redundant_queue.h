#pragma once

#include <vector>

template <typename T>
class redundant_queue {
public:
    // Adds an item to the queue with a certain redundancy
    void push(T const& msg, int redundancy);

    // Returns a vector of all the items in the queue and decrements the redundancy for them
    auto pop() -> std::vector<T>;

    // Returns a vector of all the items in the queue without decrementing the redundancy
    auto peek() const -> std::vector<T>;

    // Removes all the elements from the queue
    void clear();

    // Returns the number of elements in the queue
    auto size() const -> unsigned {
        return data.size();
    }
private:
    std::vector<std::tuple<T, int>> data;
};
