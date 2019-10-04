#pragma once

#include <string>

// Sets the path of the logfile to write to
void init_logging(std::string log_file_path);
// Adds a log line to the log file
void log(std::string data);
