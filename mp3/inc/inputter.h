#pragma once

#include <functional>
#include <optional>

template <typename T>
class inputter {
public:
    inputter(std::function<std::optional<T>()> source_) : source(source_) {}
    virtual ~inputter() {}

    class iterator {
    public:
        iterator(std::function<std::optional<T>()> source_) : source(source_), end(false) {
            generate();
        }
        iterator() : end(true) {}
        bool operator==(const iterator &a) {
            return a.end == end;
        }
        bool operator!=(const iterator &a) {
            return a.end != end;
        }
        T &operator*() {
            return curval;
        }
        T *operator->() {
            return &curval;
        }
        iterator &operator++() {
            generate();
            return *this;
        }

    private:
        void generate() {
            std::optional<T> op = source();
            if (op) {
                curval = op.value();
            } else {
                end = true;
            }
        }

        T curval;
        std::function<std::optional<T>()> source;
        bool end;
    };

    iterator begin() {
        return iterator(source);
    }

    iterator end() {
        return iterator();
    }

private:
    std::function<std::optional<T>()> source;
};
