#pragma once

#include "sdfs_master.h"
#include "sdfs_client.h"
#include "mock_sdfs_client.h"
#include "environment.h"

#include <vector>
#include <string>

class mock_sdfs_master : public sdfs_master, public service_impl<mock_sdfs_master> {
public:
    mock_sdfs_master(environment &env);

    void start() {}
    void stop() {}
    std::vector<std::string> get_files_by_prefix(std::string prefix);

private:
    mock_sdfs_client *client;
};
