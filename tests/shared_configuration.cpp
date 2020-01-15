#include "test.h"
#include "shared_configuration.h"
#include "environment.h"
#include "configuration.h"
#include "sdfs_client.h"

#include <cassert>
#include <optional>
#include <vector>
#include <string>

testing::register_test register_multiple_vals(
    "shared_configuration.multiple_vals",
    "Tests reading and writing multiple values to the shared configuration",
    3, [] (logger::log_level level)
{
    unsigned NUM_NODES = 5;

    environment_group env_group(true);
    auto envs = env_group.get_envs(NUM_NODES);

    // We initialize the environments knowing that mock_sdfs will be used,
    // which doesn't require any other services but configuration to be present
    for (auto const& env : envs) {
        configuration *config = env->get<configuration>();
        config->set_dir("./dir/");
        config->set_sdfs_subdir("sdfs");
        config->set_shared_config_subdir("shared_config");

        env->get<shared_configuration>()->start();
    }

    // Write unique key/value pairs from each environment
    for (unsigned i = 0; i < NUM_NODES; i++) {
        envs[i]->get<shared_configuration>()->set_value("key" + std::to_string(i), "value" + std::to_string(i));
    }

    // Check that each environment can read each key-value pair
    for (unsigned i = 0; i < NUM_NODES; i++) {
        for (unsigned j = 0; j < NUM_NODES; j++) {
            std::optional<std::string> val_opt = envs[j]->get<shared_configuration>()->get_value("key" + std::to_string(i));
            assert(val_opt && "Failed to retrieve value from configuration");
            assert(val_opt.value() == "value" + std::to_string(i) && "Retrieved incorrect value from configuration");
        }
    }

    for (unsigned i = 0; i < NUM_NODES; i++) {
        envs[i]->get<shared_configuration>()->stop();
    }
});

testing::register_test register_watch(
    "shared_configuration.watch_value",
    "Tests that watching values works as expected",
    15, [] (logger::log_level level)
{
    environment_group env_group(true);
    auto envs = env_group.get_envs(2);

    // We initialize the environments knowing that mock_sdfs will be used,
    // which doesn't require any other services but configuration to be present
    for (auto const& env : envs) {
        configuration *config = env->get<configuration>();
        config->set_dir("./dir/");
        config->set_sdfs_subdir("sdfs");
        config->set_shared_config_subdir("shared_config");

        env->get<shared_configuration>()->start();
    }

    // Watch for the key "watch" from one of the environments
    unsigned counter = 0;
    envs[1]->get<shared_configuration>()->watch_value("watch", [&] (std::string value) {
        assert(value == std::to_string(counter++) && "Unexpected update from watch_value");
    });

    // Write values to trigger watch_value only sometimes
    shared_configuration *sc = envs[0]->get<shared_configuration>();
    sc->set_value("watch", "0"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("watch", "0"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("abcde", "!"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("watch", "1"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("abcde", "!"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("watch", "2"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("abcde", "!"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("watch", "2"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("watch", "3"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    sc->set_value("watch", "4"); std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    assert(counter == 5 && "watch_value did not receive all updates");

    for (unsigned i = 0; i < 2; i++) {
        envs[i]->get<shared_configuration>()->stop();
    }
});
