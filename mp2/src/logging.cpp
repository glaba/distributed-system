#include "logging.h"

#include <iostream>

std::string path = "";
bool verbose = false;

// Sets the path of the logfile to write to
void init_logging(std::string log_file_path, bool verbose_) {
	path = log_file_path;
	verbose = verbose_;
}

// Adds a log line to the log file
void log(std::string data) {
	// For now
	std::cout << data << std::endl;
}

// Adds a verbose log line to the log file if verbose logging is enabled
void log_v(std::string data) {
	// For now
	if (verbose)
		std::cout << data << std::endl;
}
