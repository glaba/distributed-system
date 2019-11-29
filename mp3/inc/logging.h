#pragma once

#include <memory>
#include <string>

class logger {
public:
    enum log_level {
        level_off, level_info, level_debug, level_trace
    };

    virtual ~logger() {}

    virtual void info(std::string data) = 0;
    virtual void debug(std::string data) = 0;
    virtual void trace(std::string data) = 0;
};

class logger_factory {
public:
    virtual void configure(logger::log_level level_, std::string log_file_path_) = 0;
    virtual void configure(logger::log_level level_) = 0;
    virtual void include_hostname() = 0;
    virtual std::unique_ptr<logger> get_logger(std::string prefix) = 0;
};
