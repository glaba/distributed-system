#pragma once

#include "service.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cassert>
#include <functional>
#include <typeinfo>
#include <vector>
#include <chrono>
#include <thread>
#include <string>
#include <iostream>
#include <cxxabi.h>
#include <atomic>
#include <random>

class environment_group;

// A class which represents the execution environment of one instance of the program
class environment {
public:
    // Creates a new environment with a fresh set of services isolated from any other environment
    environment(bool is_test_env_)
        : is_test_env(is_test_env_)
    {
        wait_for_registry();

        std::lock_guard<std::mutex> guard(rand_mutex);
        env_id = mt_rand();
    }

    // Returns a pointer to the service of type T
    template <typename T>
    T *get() {
        std::string id = abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);

        spawning_mutex.lock();

        // If it is not being constructed / done being constructed already, we will construct the service ourselves
        if (spawning_services.find(id) == spawning_services.end()) {
            spawning_services.insert(id);

            spawning_mutex.unlock();

            std::unique_ptr<service> svc;
            { // Atomic access to the service
                std::lock_guard<std::recursive_mutex> guard_registry(is_test_env ? test_service_registry_mutex : service_registry_mutex);
                auto &registry = is_test_env ? test_service_registry : service_registry;

                svc = registry[id](*this);
            }

            std::lock_guard<std::mutex> guard(services_mutex);
            services[id] = std::move(svc);

            // Initialize the state if needed
            std::lock_guard<std::mutex> guard_state(service::state_mutex);

            services[id]->env_id = env_id;

            // Service state uses the impl ID instead of the intf ID because each impl has different shared state
            std::string impl_id = services[id]->get_impl_id();
            if (service::state[env_id].find(impl_id) == service::state[env_id].end()) {
                service::state[env_id][impl_id] = services[id]->init_state();
            }

        // If it's already being constructed / done being constructed, we just poll while waiting for the service to appear
        } else {
            spawning_mutex.unlock();

            // Poll while trying to obtain the service
            while (true) {
                {
                    std::lock_guard<std::mutex> guard(services_mutex);
                    if (services.find(id) != services.end()) {
                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        std::lock_guard<std::mutex> guard(services_mutex);
        T *retval = dynamic_cast<T*>(services[id].get());
        assert(retval != nullptr);
        return retval;
    }

private:
    environment(bool is_test_env_, uint32_t env_id_)
        : is_test_env(is_test_env_), env_id(env_id_)
    {
        wait_for_registry();
    }

    void wait_for_registry();

    static void add_to_service_registry(std::string id, std::function<std::unique_ptr<service>(environment&)> callback);
    static void add_to_test_service_registry(std::string id, std::function<std::unique_ptr<service>(environment&)> callback);

    bool is_test_env;
    uint32_t env_id;

    std::mutex services_mutex;
    std::unordered_map<std::string, std::unique_ptr<service>> services;

    std::mutex spawning_mutex;
    std::unordered_set<std::string> spawning_services; // A set containing services that are being spun up

    // The start time of the program, which will be used to decide when the registry has been fully populated
    static uint32_t start_time;

    static std::mutex rand_mutex;
    static std::mt19937 mt_rand;

    // A map from service name to a factory that will construct an instance of that service (normal and test)
    static std::recursive_mutex service_registry_mutex;
    static std::unordered_map<std::string, std::function<std::unique_ptr<service>(environment&)>> service_registry;

    static std::recursive_mutex test_service_registry_mutex;
    static std::unordered_map<std::string, std::function<std::unique_ptr<service>(environment&)>> test_service_registry;

    template <typename Intf, typename Impl>
    friend class register_service;
    template <typename Intf, typename Impl>
    friend class register_test_service;
    friend class service;
    friend class environment_group;
};

class environment_group {
public:
    environment_group(bool is_test_env_)
        : is_test_env(is_test_env_)
    {
        std::lock_guard<std::mutex> guard(environment::rand_mutex);
        env_id = environment::mt_rand();
    }

    std::unique_ptr<environment> get_env() {
        return std::unique_ptr<environment>(new environment(is_test_env, env_id));
    }

    std::vector<std::unique_ptr<environment>> get_envs(unsigned n) {
        std::vector<std::unique_ptr<environment>> retval;
        for (unsigned i = 0; i < n; i++) {
            retval.push_back(get_env());
        }
        return retval;
    }

private:
    bool is_test_env;
    uint32_t env_id;
};

// Instantiate this class in order to register a service
template <typename Intf, typename Impl>
class register_service {
public:
    register_service() {
        std::lock_guard<std::recursive_mutex> guard(environment::service_registry_mutex);

        environment::add_to_service_registry(abi::__cxa_demangle(typeid(Intf).name(), 0, 0, 0), [] (environment &env) {
            return std::unique_ptr<service>(static_cast<service*>(new Impl(env)));
        });
    }
    register_service(std::function<std::unique_ptr<service>(environment&)> callback) {
        std::lock_guard<std::recursive_mutex> guard(environment::service_registry_mutex);

        environment::add_to_service_registry(abi::__cxa_demangle(typeid(Intf).name(), 0, 0, 0), callback);
    }
};

// Instantiate this class in order to register a service for tests
template <typename Intf, typename Impl>
class register_test_service {
public:
    register_test_service() {
        std::lock_guard<std::recursive_mutex> guard(environment::test_service_registry_mutex);

        environment::add_to_test_service_registry(abi::__cxa_demangle(typeid(Intf).name(), 0, 0, 0), [] (environment &env) {
            return std::unique_ptr<service>(static_cast<service*>(new Impl(env)));
        });
    }
    register_test_service(std::function<std::unique_ptr<service>(environment&)> callback) {
        std::lock_guard<std::recursive_mutex> guard(environment::test_service_registry_mutex);

        environment::add_to_test_service_registry(abi::__cxa_demangle(typeid(Intf).name(), 0, 0, 0), callback);
    }
};

// Instantiate this class in order to register a service for both normal execution and tests
template <typename Intf, typename Impl>
class register_auto {
public:
    register_auto() {
        register_service_ = std::make_unique<register_service<Intf, Impl>>();
        register_test_service_ = std::make_unique<register_test_service<Intf, Impl>>();
    }
    register_auto(std::function<std::unique_ptr<Impl>(environment&)> callback) {
        register_service_ = std::make_unique<register_service<Intf, Impl>>(callback);
        register_test_service_ = std::make_unique<register_test_service<Intf, Impl>>(callback);
    }

private:
    std::unique_ptr<register_service<Intf, Impl>> register_service_;
    std::unique_ptr<register_test_service<Intf, Impl>> register_test_service_;
};
