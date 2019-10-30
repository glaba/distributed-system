#include "logging.h"

#include <iostream>
#include <chrono>
#include <cstring>

std::mutex logger::log_mutex;

// Adds a log line to the log file
void logger::log(std::string data) {
    std::lock_guard<std::mutex> guard(log_mutex);

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);

    std::string msg = std::string("[") + std::strtok(ctime(&tt), "\n") + std::string("] ") +
        prefix + std::string(": ") + data + std::string("\n");

    (use_stdout ? std::cout : log_stream) << msg;
}

// Adds a verbose log line to the log file if verbose logging is enabled
void logger::log_v(std::string data) {
    if (verbose) {
        log(data);
    }
}
