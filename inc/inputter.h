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
        iterator(std::function<std::optional<T>()> const& source_) : source(source_), end(false) {
            generate();
        }
        iterator() : end(true) {}
        auto operator==(iterator const& a) const -> bool {
            return a.end == end;
        }
        auto operator!=(iterator const& a) const -> bool {
            return a.end != end;
        }
        auto operator*() -> T& {
            return curval;
        }
        auto operator->() const -> T*{
            return &curval;
        }
        auto operator++() -> iterator& {
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

    auto begin() const -> iterator {
        return iterator(source);
    }

    auto end() const -> iterator {
        return iterator();
    }

private:
    std::function<std::optional<T>()> source;
};
