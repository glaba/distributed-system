#pragma once

#include "logging.h"
#include "configuration.h"
#include "environment.h"
#include "service.h"

#include <string>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <memory>
#include <cassert>

class logger_factory_impl : public logger_factory, public service_impl<logger_factory_impl> {
public:
    logger_factory_impl(environment &env)
        : config(env.get<configuration>()) {}

    void configure(logger::log_level level_, std::string log_file_path_);
    void configure(logger::log_level level_);
    void include_hostname();
    std::unique_ptr<logger> get_logger(std::string prefix);

protected:
    std::mutex logger_factory_mutex;
    logger::log_level level;
    std::string log_file_path;
    bool using_stdout;
    bool including_hostname;

    configuration *config;

    static std::mutex log_mutex;

    template <logger::log_level level>
    class stdout_logger : public logger {
    public:
        stdout_logger(std::string prefix_)
            : prefix(prefix_) {}

        void info(std::string data);
        void debug(std::string data);
        void trace(std::string data);

    private:
        void log(std::string data);

        std::string prefix;
    };

    template <logger::log_level level>
    class file_logger : public logger {
    public:
        file_logger(std::string log_file_path_, std::string prefix_)
            : prefix(prefix_), log_stream((log_file_path_ == "") ? "/dev/null" : log_file_path_) {}

        void info(std::string data);
        void debug(std::string data);
        void trace(std::string data);

    private:
        void log(std::string data);

        std::string prefix;
        std::ofstream log_stream;
    };
};

class test_logger_factory_impl : public logger_factory_impl {
public:
    test_logger_factory_impl(environment &env)
        : logger_factory_impl(env)
    {
        include_hostname();
    }
};