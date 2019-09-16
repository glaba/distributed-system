#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <vector>
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

using std::vector;
using std::string;

/* functions */
string read_string(int socket, size_t num_bytes);
string query_machine(string ip, string port, string query_string);
