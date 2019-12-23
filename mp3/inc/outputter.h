#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <optional>

template <typename T>
class outputter {
public:
    outputter(std::function<std::optional<T>()> sink_) : sink(sink_) {}
    virtual ~outputter() {}

    class output_object {
    public:
        // The function sink will be called whenever output_object is assigned a value of type T
        // This is guaranteed to be called at most once, throwing a runtime exception if it is called more
        output_object(std::function<void(T&)> sink_) : called(false), complete(false), sink(sink_) {}
        output_object &operator=(T &obj) {
            assert(!called && "Outputter iterator should only be assigned to once, as it is not idempotent");
            complete = sink(obj);
            called = true;
        }
        bool is_complete() {
            return complete;
        }
    private:
        bool called, complete;
        std::function<void(T&)> sink;
    };

    class iterator {
    public:
        iterator(std::function<std::optional<T>()> sink_) : sink(sink_), end(false) {}
        iterator() : end(true) {}
        bool operator==(const iterator &a) {
            return a.end == end;
        }
        bool operator!=(const iterator &a) {
            return a.end != end;
        }
        output_object &operator*() {
            curval = output_object(sink);
            return curval;
        }
        iterator &operator++() {
            end = curval.is_complete();
            return *this;
        }

    private:
        output_object curval;
        std::function<std::optional<T>()> sink;
        bool end;
    };

    iterator begin() {
        return iterator(sink);
    }

    iterator end() {
        return iterator();
    }

    // Processes a single line of the output of the executable
    virtual bool process_line(std::string line) = 0;
    // Resets any internal state accumulated from processing lines
    virtual void reset() = 0;
    // Returns the output file to append to as well as a vector of the values to append to the file
    virtual std::pair<std::string, std::vector<std::string>> emit() = 0;
    // Whether or not there is more data to emit
    virtual bool more() = 0;

private:
    std::function<std::optional<T>()> sink;
};
