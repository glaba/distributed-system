#include "logging.h"

#include <iostream>
#include <chrono>
#include <cstring>

// Adds a log line to the log file
void logger::log(std::string data) {
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	time_t tt = std::chrono::system_clock::to_time_t(now);

    (use_stdout ? std::cout : log_stream) << "[" << std::strtok(ctime(&tt), "\n") << "] " << prefix << ": " << data << std::endl;
}

// Adds a verbose log line to the log file if verbose logging is enabled
void logger::log_v(std::string data) {
	if (verbose) {
		log(data);
	}
}
