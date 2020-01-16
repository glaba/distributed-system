#pragma once

#include "sdfs_server.h"
#include "environment.h"

class mock_sdfs_server : public sdfs_server, public service_impl<mock_sdfs_server> {
public:
    mock_sdfs_server(environment &env) {}
    // Starts accepting and processing client requests
    void start() {};
    // Stops all server logic for the filesystem
    void stop() {};
};
