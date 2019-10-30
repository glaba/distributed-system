#pragma once

#include <string>
#include <cstdio>
#include <fstream>
#include <mutex>

class logger {
public:
    logger(std::string log_file_path_, std::string prefix_, bool verbose_)
        : use_stdout(false), log_file_path((log_file_path_ == "") ? "/dev/null" : log_file_path_),
          prefix(prefix_), verbose(verbose_), log_stream(log_file_path) {}

    logger(std::string prefix_, bool verbose_)
        : use_stdout(true), prefix(prefix_), verbose(verbose_) {}

    // Adds a log line to the log file
    void log(std::string data);
    // Adds a verbose log line to the log file if verbose logging is enabled
    void log_v(std::string data);

    bool using_stdout() {
        return use_stdout;
    }

    bool is_verbose() {
        return verbose;
    }
private:
    bool use_stdout;
    std::string log_file_path;
    std::string prefix;
    bool verbose;

    static std::mutex log_mutex;

    std::ofstream log_stream;
};
