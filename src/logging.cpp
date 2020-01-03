#include "logging.h"
#include "logging.hpp"

#include <iostream>
#include <chrono>
#include <cstring>

using std::unique_ptr;
using std::make_unique;

std::mutex logger_factory_impl::log_mutex;

template <logger::log_level level>
inline void logger_factory_impl::stdout_logger<level>::log(std::string const& data, std::string const& log_level_char) const {
    std::lock_guard<std::mutex> guard(log_mutex);

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);

    std::string msg = std::string("[") + std::strtok(ctime(&tt), "\n") + std::string("] ") +
        log_level_char + " " + prefix + std::string(": ") + data;

    std::cout << msg << std::endl;
}

template <logger::log_level level>
void logger_factory_impl::stdout_logger<level>::info(std::string const& data) const {
    if (level == level_info || level == level_debug || level == level_trace) {
        log(data, "I");
    }
}

template <logger::log_level level>
void logger_factory_impl::stdout_logger<level>::debug(std::string const& data) const {
    if (level == level_debug || level == level_trace) {
        log(data, "D");
    }
}

template <logger::log_level level>
void logger_factory_impl::stdout_logger<level>::trace(std::string const& data) const {
    if (level == level_trace) {
        log(data, "T");
    }
}

template <logger::log_level level>
inline void logger_factory_impl::file_logger<level>::log(std::string const& data, std::string const& log_level_char) const {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);

    std::string msg = std::string("[") + std::strtok(ctime(&tt), "\n") + std::string("] ") +
        log_level_char + " " + prefix + std::string(": ") + data;

    log_stream << msg << std::endl;
}

template <logger::log_level level>
void logger_factory_impl::file_logger<level>::info(std::string const& data) const {
    if (level == level_info || level == level_debug || level == level_trace) {
        log(data, "I");
    }
}

template <logger::log_level level>
void logger_factory_impl::file_logger<level>::debug(std::string const& data) const {
    if (level == level_debug || level == level_trace) {
        log(data, "D");
    }
}

template <logger::log_level level>
void logger_factory_impl::file_logger<level>::trace(std::string const& data) const {
    if (level == level_trace) {
        log(data, "T");
    }
}

void logger_factory_impl::configure(logger::log_level level_, std::string const& log_file_path_) {
    std::lock_guard<std::mutex> guard(logger_factory_mutex);
    level = level_;
    log_file_path = log_file_path_;
    using_stdout = false;
}

void logger_factory_impl::configure(logger::log_level level_) {
    std::lock_guard<std::mutex> guard(logger_factory_mutex);
    level = level_;
    using_stdout = true;
}

void logger_factory_impl::include_hostname() {
    std::lock_guard<std::mutex> guard(logger_factory_mutex);
    including_hostname = true;
}

auto logger_factory_impl::get_logger(std::string const& prefix) const -> unique_ptr<logger> {
    std::lock_guard<std::mutex> guard(logger_factory_mutex);

    std::string full_prefix = (including_hostname) ? (config->get_hostname() + " " + prefix) : prefix;
    if (using_stdout) {
        switch (level) {
            case logger::log_level::level_off: return make_unique<stdout_logger<logger::log_level::level_off>>(full_prefix);
            case logger::log_level::level_info: return make_unique<stdout_logger<logger::log_level::level_info>>(full_prefix);
            case logger::log_level::level_debug: return make_unique<stdout_logger<logger::log_level::level_debug>>(full_prefix);
            case logger::log_level::level_trace: return make_unique<stdout_logger<logger::log_level::level_trace>>(full_prefix);
            default: assert(false && "Invalid enum value");
        }
    } else {
        switch (level) {
            case logger::log_level::level_off: return make_unique<file_logger<logger::log_level::level_off>>(log_file_path, full_prefix);
            case logger::log_level::level_info: return make_unique<file_logger<logger::log_level::level_info>>(log_file_path, full_prefix);
            case logger::log_level::level_debug: return make_unique<file_logger<logger::log_level::level_debug>>(log_file_path, full_prefix);
            case logger::log_level::level_trace: return make_unique<file_logger<logger::log_level::level_trace>>(log_file_path, full_prefix);
            default: assert(false && "Invalid enum value");
        }
    }
}

register_service<logger_factory, logger_factory_impl> register_logger_factory;
register_test_service<logger_factory, test_logger_factory_impl> register_test_logger_factory;
