#include "redundant_queue.h"
#include "member_list.h"
#include "election_messages.h"

#include <tuple>

// Adds an item to the queue with a certain redundancy
template <typename T>
void redundant_queue<T>::push(T const& item, int redundancy) {
    data.push_back({item, redundancy});
}

// Returns a vector of all the items in the queue and decrements the redundancy for them
template <typename T>
auto redundant_queue<T>::pop() -> std::vector<T> {
    std::vector<T> ret;

    // Pop all the items off, decrement redundancy and remove those with redundancy 0
    auto k = std::begin(data);
    while (k != std::end(data)) {
        auto &[item, counter] = *k;

        ret.push_back(item);
        counter--;

        if (counter <= 0) {
            k = data.erase(k);
        } else {
            k++;
        }
    }

    return ret;
}

// Returns a vector of all the items in the queue without decrementing the redundancy
template <typename T>
auto redundant_queue<T>::peek() const -> std::vector<T> {
    std::vector<T> ret;

    for (auto const& [item, _] : data) {
        ret.push_back(item);
    }

    return ret;
}

template <typename T>
void redundant_queue<T>::clear() {
    data.clear();
}

// Force the compiler to compile the required templates
template class redundant_queue<uint32_t>;
template class redundant_queue<member>;
template class redundant_queue<std::tuple<std::string, std::string>>;
template class redundant_queue<std::tuple<char*, unsigned>>;
