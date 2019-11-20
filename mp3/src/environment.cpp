#include "environment.h"

using namespace std::chrono;

uint32_t environment::start_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

std::mutex                                                                                    environment::rand_mutex;
std::mt19937                                                                                  environment::mt_rand;
std::recursive_mutex                                                                          environment::service_registry_mutex;
std::unordered_map<std::string, std::function<std::unique_ptr<service>(environment&)>>        environment::service_registry;
std::recursive_mutex                                                                          environment::test_service_registry_mutex;
std::unordered_map<std::string, std::function<std::unique_ptr<service>(environment&)>>        environment::test_service_registry;

void environment::add_to_service_registry(std::string id, std::function<std::unique_ptr<service>(environment&)> callback) {
    std::thread add_thread([id, callback] {
        // Wait while the mutex and the unordered_map get default initialized
        std::this_thread::sleep_for(milliseconds(250));

        std::lock_guard<std::recursive_mutex> guard(service_registry_mutex);
        service_registry[id] = callback;
    });
    add_thread.detach();
}

void environment::add_to_test_service_registry(std::string id, std::function<std::unique_ptr<service>(environment&)> callback) {
    std::thread add_thread([id, callback] {
        // Wait while the mutex and the unordered_map get default initialized
        std::this_thread::sleep_for(milliseconds(250));

        std::lock_guard<std::recursive_mutex> guard(test_service_registry_mutex);
        test_service_registry[id] = callback;
    });
    add_thread.detach();
}

void environment::wait_for_registry() {
    uint32_t now = start_time;
    while (now - start_time < 500) {
        now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        std::this_thread::sleep_for(milliseconds(10));
    }
}
