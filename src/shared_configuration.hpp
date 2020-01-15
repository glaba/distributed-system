#pragma once

#include "shared_configuration.h"
#include "heartbeater.h"
#include "environment.h"
#include "service.h"
#include "sdfs_client.h"
#include "logging.h"
#include "locking.h"
#include "configuration.h"

#include <string>
#include <optional>
#include <vector>

class shared_configuration_impl : public shared_configuration, public service_impl<shared_configuration_impl> {
public:
    shared_configuration_impl(environment &env);

    void start();
    void stop();

    void set_value(std::string const& key, std::string const& value);
    auto get_value(std::string const& key) -> std::optional<std::string>;
    void watch_value(std::string const& key, std::function<void(std::string const&)> const& callback);

private:
    std::atomic<bool> running, completed;

    using callback_map = std::unordered_map<std::string, std::vector<std::function<void(std::string const&)>>>;
    locked<callback_map> cb_map_lock;

    std::unordered_map<std::string, std::string> cached_values;

    sdfs_client *sdfsc;
    configuration *config;
    std::unique_ptr<logger> lg;
};