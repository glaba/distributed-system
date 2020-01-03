#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <optional>

template <typename T>
class outputter {
public:
    outputter(std::function<void(T const&)> const& sink_) : sink(sink_) {}
    virtual ~outputter() {}

    class output_object {
    public:
        // The function sink will be called whenever output_object is assigned a value of type T
        // This is guaranteed to be called at most once, throwing a runtime exception if it is called more
        output_object(std::function<void(T const&)> const& sink_) : called(false), complete(false), sink(sink_) {}
        auto operator=(T &obj) -> output_object& {
            assert(!called && "Outputter iterator should only be assigned to once, as it is not idempotent");
            complete = sink(obj);
            called = true;
        }
        auto is_complete() -> bool {
            return complete;
        }
    private:
        bool called, complete;
        std::function<void(T const&)> const& sink;
    };

    class iterator {
    public:
        iterator(std::function<void(T const&)> &sink_) : sink(sink_), end(false) {}
        iterator() : end(true) {}
        auto operator==(const iterator &a) -> bool {
            return a.end == end;
        }
        auto operator!=(const iterator &a) -> bool {
            return a.end != end;
        }
        auto operator*() -> output_object& {
            curval = output_object(sink);
            return curval;
        }
        auto operator++() -> iterator& {
            end = curval.is_complete();
            return *this;
        }

    private:
        output_object curval;
        std::function<void(T const&)> sink;
        bool end;
    };

    auto begin() -> iterator {
        return iterator(sink);
    }

    auto end() -> iterator {
        return iterator();
    }

private:
    std::function<void(T const&)> sink;
};
