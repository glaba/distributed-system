#include "logging.h"
#include "logging.hpp"

#include <iostream>
#include <chrono>
#include <cstring>

using std::string;
using std::unique_ptr;
using std::make_unique;

std::mutex logger_factory_impl::log_mutex;

template <logger::log_level level>
inline void logger_factory_impl::stdout_logger<level>::log(string const& data, string const& log_level_char) const {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    string time = string(ctime(&tt));

    std::lock_guard<std::mutex> guard(log_mutex);
    string msg = string("[") + time.substr(0, time.length() - 1) + string("] ") +
        log_level_char + " " + prefix + string(": ") + data;

    std::cout << msg << std::endl;
}

template <logger::log_level level>
void logger_factory_impl::stdout_logger<level>::info(string const& data) const {
    if (level == level_info || level == level_debug || level == level_trace) {
        log(data, "I");
    }
}

template <logger::log_level level>
void logger_factory_impl::stdout_logger<level>::debug(string const& data) const {
    if (level == level_debug || level == level_trace) {
        log(data, "D");
    }
}

template <logger::log_level level>
void logger_factory_impl::stdout_logger<level>::trace(string const& data) const {
    if (level == level_trace) {
        log(data, "T");
    }
}

template <logger::log_level level>
inline void logger_factory_impl::file_logger<level>::log(string const& data, string const& log_level_char) const {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    string time = string(ctime(&tt));

    std::lock_guard<std::mutex> guard(log_mutex);
    string msg = string("[") + time.substr(0, time.length() - 1) + string("] ") +
        log_level_char + " " + prefix + string(": ") + data;

    log_stream << msg << std::endl;
}

template <logger::log_level level>
void logger_factory_impl::file_logger<level>::info(string const& data) const {
    if (level == level_info || level == level_debug || level == level_trace) {
        log(data, "I");
    }
}

template <logger::log_level level>
void logger_factory_impl::file_logger<level>::debug(string const& data) const {
    if (level == level_debug || level == level_trace) {
        log(data, "D");
    }
}

template <logger::log_level level>
void logger_factory_impl::file_logger<level>::trace(string const& data) const {
    if (level == level_trace) {
        log(data, "T");
    }
}

void logger_factory_impl::configure(logger::log_level level_, string const& log_file_path_) {
    unlocked<logging_state> lg_state = lg_state_lock();
    lg_state->level = level_;
    lg_state->log_file_path = log_file_path_;
    lg_state->using_stdout = false;
}

void logger_factory_impl::configure(logger::log_level level_) {
    unlocked<logging_state> lg_state = lg_state_lock();
    lg_state->level = level_;
    lg_state->using_stdout = true;
}

void logger_factory_impl::include_hostname() {
    unlocked<logging_state> lg_state = lg_state_lock();
    lg_state->including_hostname = true;
}

auto logger_factory_impl::get_logger(string const& prefix) const -> unique_ptr<logger> {
    unlocked<logging_state> lg_state = lg_state_lock();

    string full_prefix = (lg_state->including_hostname) ? (config->get_hostname() + " " + prefix) : prefix;
    if (lg_state->using_stdout) {
        switch (lg_state->level) {
            case logger::log_level::level_off:
                return make_unique<stdout_logger<logger::log_level::level_off>>(full_prefix);
            case logger::log_level::level_info:
                return make_unique<stdout_logger<logger::log_level::level_info>>(full_prefix);
            case logger::log_level::level_debug:
                return make_unique<stdout_logger<logger::log_level::level_debug>>(full_prefix);
            case logger::log_level::level_trace:
                return make_unique<stdout_logger<logger::log_level::level_trace>>(full_prefix);
            default: assert(false && "Invalid enum value");
        }
    } else {
        switch (lg_state->level) {
            case logger::log_level::level_off:
                return make_unique<file_logger<logger::log_level::level_off>>(lg_state->log_file_path, full_prefix);
            case logger::log_level::level_info:
                return make_unique<file_logger<logger::log_level::level_info>>(lg_state->log_file_path, full_prefix);
            case logger::log_level::level_debug:
                return make_unique<file_logger<logger::log_level::level_debug>>(lg_state->log_file_path, full_prefix);
            case logger::log_level::level_trace:
                return make_unique<file_logger<logger::log_level::level_trace>>(lg_state->log_file_path, full_prefix);
            default: assert(false && "Invalid enum value");
        }
    }
}

register_service<logger_factory, logger_factory_impl> register_logger_factory;
register_test_service<logger_factory, test_logger_factory_impl> register_test_logger_factory;
