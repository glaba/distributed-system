#include "redundant_queue.h"
#include "member_list.h"

#include <tuple>

// Adds an item to the queue with a certain redundancy
template <typename T>
void redundant_queue<T>::push(T item, int redundancy) {
    data.push_back(std::make_tuple(item, redundancy));
}

// Returns a vector of all the items in the queue and decrements the redundancy for them
template <typename T>
std::vector<T> redundant_queue<T>::pop() {
    std::vector<T> ret;

    // Pop all the items off, decrement redundancy and remove those with redundancy 0
    auto k = std::begin(data);
    while (k != std::end(data)) {
        ret.push_back(std::get<0>(*k));
        std::get<1>(*k) = std::get<1>(*k) - 1;

        if (std::get<1>(*k) <= 0) {
            k = data.erase(k);
        } else {
            k++;
        }
    }

    return ret;
}

// Returns a vector of all the items in the queue without decrementing the redundancy
template <typename T>
std::vector<T> redundant_queue<T>::peek() {
    std::vector<T> ret;

    for (auto k : data) {
        ret.push_back(std::get<0>(k));
    }

    return ret;
}

// Force the compiler to compile the required templates
template class redundant_queue<uint32_t>;
template class redundant_queue<member>;
