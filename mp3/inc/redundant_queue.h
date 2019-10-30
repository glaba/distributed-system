#pragma once

#include <vector>

template <typename T>
class redundant_queue {
public:
    // Adds an item to the queue with a certain redundancy
    void push(T msg, int redundancy);

    // Returns a vector of all the items in the queue and decrements the redundancy for them
    std::vector<T> pop();

    // Returns a vector of all the items in the queue without decrementing the redundancy
    std::vector<T> peek();

    // Returns the number of elements in the queue
    unsigned size() {
        return data.size();
    }
private:
    std::vector<std::tuple<T, int>> data;
};
