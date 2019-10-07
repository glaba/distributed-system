#include "logging.h"

#include <iostream>

// Adds a log line to the log file
void logger::log(std::string data) {
	// For now
	std::cout << data << std::endl;
}

// Adds a verbose log line to the log file if verbose logging is enabled
void logger::log_v(std::string data) {
	// For now
	if (verbose)
		std::cout << data << std::endl;
}
