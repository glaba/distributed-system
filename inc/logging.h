#pragma once

#include <memory>
#include <string>

class logger {
public:
    enum log_level {
        level_off, level_info, level_debug, level_trace
    };

    virtual ~logger() {}

    virtual void info(std::string const& data) const = 0;
    virtual void debug(std::string const& data) const = 0;
    virtual void trace(std::string const& data) const = 0;
};

class logger_factory {
public:
    virtual void configure(logger::log_level level_, std::string const& log_file_path_) = 0;
    virtual void configure(logger::log_level level_) = 0;
    virtual void include_hostname() = 0;
    virtual auto get_logger(std::string const& prefix) const -> std::unique_ptr<logger> = 0;
};
