#pragma once

#include <memory>
#include <functional>
#include <mutex>
#include <typeinfo>
#include <cxxabi.h>
#include <chrono>
#include <memory>
#include <iostream>
#include <thread>

#include "locking.h"

class service_state {
public:
    service_state() {}
    virtual ~service_state() {}
};

template <typename Impl>
class service_impl;

// Services are classes that are singletons within each environment that serve some function in the program
// Services should not do anything upon instantiation, but should have methods to allow them to perform functionality
class service {
public:
    virtual ~service() {}

    virtual auto get_service_id() const -> std::string = 0;

protected:
    // Returns a service_state object that represents the default state of the service
    virtual auto init_state() -> std::unique_ptr<service_state> {
        return std::make_unique<service_state>();
    }

private:
    // The ID of the environment containing this service, which will be set by environment upon instantiation
    uint32_t env_id = 0;

    // Per environment group, a map from service IMPLEMENTATION name to the state for that service
    // This allows services in different environments but the same environment group to share state
    using state_map = std::unordered_map<std::string, locked<service_state>>;
    using global_state_map = std::unordered_map<uint32_t, state_map>;
    static locked<global_state_map> global_states_lock;

    friend class environment;
    template <typename Impl>
    friend class service_impl;
};

template <typename Impl>
class service_impl : public service {
public:
    virtual ~service_impl() {}

    auto get_service_id() const -> std::string final {
        return abi::__cxa_demangle(typeid(Impl).name(), 0, 0, 0);
    }

protected:
    // Gives atomic access to the service_state object for the singleton environment or environment group
    auto access_state() -> unlocked<service_state> {
        std::string id = get_service_id();

        std::optional<unlocked<service_state>> state_opt;

        while (true) {
            {
                unlocked<global_state_map> global_states = service::global_states_lock();
                if (env_id != 0 && (*global_states)[env_id].find(id) != (*global_states)[env_id].end()) {
                    locked<service_state> const& state_lock = (*global_states)[env_id][id];
                    state_opt = state_lock();
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return std::move(state_opt.value());
    }

    friend class environment;
};
