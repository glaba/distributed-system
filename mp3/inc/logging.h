#pragma once

#include <string>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <cassert>

class logger {
public:
    enum log_level {
        level_off, level_info, level_debug, level_trace
    };

    logger(std::string log_file_path_, std::string prefix_, log_level level_)
        : use_stdout(false), log_file_path((log_file_path_ == "") ? "/dev/null" : log_file_path_),
          prefix(prefix_), level(level_), log_stream(log_file_path) {}

    logger(std::string prefix_, log_level level_)
        : use_stdout(true), prefix(prefix_), level(level_) {}

    logger(std::string prefix_, logger &l_)
        : use_stdout(true), prefix(prefix_) {
        assert(l_.use_stdout == true);

        level = l_.level;
    }

    logger(std::string log_file_path_, std::string prefix_, logger &l_)
        : use_stdout(false), log_file_path((log_file_path_ == "") ? "/dev/null" : log_file_path_),
          prefix(prefix_), log_stream(log_file_path) {

        assert(l_.use_stdout == false);

        level = l_.level;
    }

    // Adds log lines at varying levels of importance
    void info(std::string data);
    void debug(std::string data);
    void trace(std::string data);

private:
    // Adds a log line to the log file
    void log(std::string data);

    bool use_stdout;
    std::string log_file_path;
    std::string prefix;
    log_level level;

    static std::mutex log_mutex;

    std::ofstream log_stream;
};
