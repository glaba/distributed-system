#include "shared_configuration.hpp"
#include "utils.h"

#include <atomic>
#include <fstream>

#define CONFIG_DIR string(".shared_config")

using std::string;
using std::optional;

shared_configuration_impl::shared_configuration_impl(environment &env)
    : sdfsc(env.get<sdfs_client>())
    , config(env.get<configuration>())
    , lg(env.get<logger_factory>()->get_logger("shared_configuration")) {}

void shared_configuration_impl::start() {
    if (running.load()) {
        return;
    }

    running = true;
    completed = false;

    sdfsc->start();
    // TODO: verify that the failure of the mkdir is due to the directory already existing, otherwise, retry
    sdfsc->mkdir(CONFIG_DIR);

    // TODO: switch from a polling implementation to a non-polling implementation
    std::thread watch_thread([this] {
        while (running.load()) {
            {
                unlocked<callback_map> cb_map = cb_map_lock();
                for (auto const& [key, callbacks] : *cb_map) {
                    optional<string> value_opt = get_value(key);

                    if (!value_opt) continue;
                    if (cached_values[key] == value_opt.value()) continue;

                    string &value = value_opt.value();
                    for (auto const& callback : callbacks) {
                        callback(value);
                    }
                    cached_values[key] = std::move(value);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        completed = true;
    });
    watch_thread.detach();
}

void shared_configuration_impl::stop() {
    running = false;
    while (!completed.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void shared_configuration_impl::set_value(string const& key, string const& value) {
    if (value.length() > max_value_len) {
        return;
    }

    utils::backoff([&] {
        return sdfsc->put(inputter<string>::constant_inputter(value), CONFIG_DIR + "/" + key) == 0;
    });
}

auto shared_configuration_impl::get_value(string const& key) -> optional<string> {
    static std::atomic<int> atomic_counter = 0;

    string local_filename = config->get_shared_config_dir() + std::to_string(atomic_counter++);
    if (sdfsc->get(local_filename, CONFIG_DIR + "/" + key) != 0) {
        return std::nullopt;
    }

    char buffer[max_value_len];

    std::ifstream src(local_filename, std::ios::binary);
    src.read(buffer, max_value_len);

    // If the stream failed or if the length of the file was longer than max_value_len
    if (src.bad() || !src.eof()) {
        return std::nullopt;
    }

    return string(buffer, src.gcount());
}

void shared_configuration_impl::watch_value(string const& key, std::function<void(string const&)> const& callback) {
    (*cb_map_lock())[key].push_back(callback);
}

register_auto<shared_configuration, shared_configuration_impl> register_shared_configuration;