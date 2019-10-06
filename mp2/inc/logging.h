#pragma once

#include <string>

// Sets the path of the logfile to write to
void init_logging(std::string log_file_path, bool verbose_);
// Adds a log line to the log file
void log(std::string data);
// Adds a verbose log line to the log file if verbose logging is enabled
void log_v(std::string data);
