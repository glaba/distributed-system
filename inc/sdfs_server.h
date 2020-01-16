#pragma once

#include <string>

class sdfs_server {
public:
    // Starts accepting and processing client requests
    virtual void start() = 0;
    // Stops all server logic for the filesystem
    virtual void stop() = 0;
};
