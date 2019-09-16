#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <array>
#include <memory>
#include <cstdio>
#include <stdexcept>

#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "utils.h"

using std::cin;
using std::cout;
using std::cerr;
using std::endl;

using std::array;
using std::string;
using std::unique_ptr;

string log_file = "~/machine.i.log";

/* functions */
int setup_server(string port);
void run_server(int server_fd);
string run_grep_command(string search_text);
