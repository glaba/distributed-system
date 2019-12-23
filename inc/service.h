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

    virtual std::string get_service_id() = 0;

protected:
    // Returns a service_state object that represents the default state of the service
    virtual std::unique_ptr<service_state> init_state() {
        return std::make_unique<service_state>();
    }

private:
    // The ID of the environment containing this service, which will be set by environment upon instantiation
    uint32_t env_id = 0;

    // Per environment group, a map from service IMPLEMENTATION name to the state for that service
    // This allows services in different environments but the same environment group to share state
    static std::mutex state_mutex;
    static std::unordered_map<uint32_t, std::unordered_map<std::string, std::unique_ptr<service_state>>> state;

    friend class environment;
    template <typename Impl>
    friend class service_impl;
};

template <typename Impl>
class service_impl : public service {
public:
    virtual ~service_impl() {}

    std::string get_service_id() final {
        return abi::__cxa_demangle(typeid(Impl).name(), 0, 0, 0);
    }

protected:
    // Gives atomic access to the service_state object for the singleton environment or environment group
    void access_state(std::function<void(service_state*)> callback) {
        std::string id = get_service_id();

        while (true) {
            {
                std::lock_guard<std::mutex> guard(service::state_mutex);
                if (env_id != 0 && service::state[env_id].find(id) != service::state[env_id].end()) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::lock_guard<std::mutex> guard(service::state_mutex);
        callback(service::state[env_id][id].get());
    }

    friend class environment;
};
