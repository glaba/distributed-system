#pragma once

#include <string>

class logger {
public:
	logger(std::string log_file_path_, bool verbose_)
		: use_stdout(false), log_file_path(log_file_path_), verbose(verbose_) {}

	logger(bool verbose_)
		: use_stdout(true), verbose(verbose_) {}
	
	// Adds a log line to the log file
	void log(std::string data);
	// Adds a verbose log line to the log file if verbose logging is enabled
	void log_v(std::string data);
private:
	bool use_stdout;
	std::string log_file_path;
	bool verbose;
};
