#pragma once

#include <string>
#include <optional>
#include <functional>

class shared_configuration {
public:
    static const unsigned max_value_len = 8192;

    virtual void start() = 0;
    virtual void stop() = 0;

    // Values cannot be longer than max_value_len
    virtual void set_value(std::string const& key, std::string const& value) = 0;
    virtual auto get_value(std::string const& key) -> std::optional<std::string> = 0;
    virtual void watch_value(std::string const& key, std::function<void(std::string const&)> const& callback) = 0;
};