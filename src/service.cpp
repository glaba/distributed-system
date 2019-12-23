#include "service.h"

std::mutex service::state_mutex;
std::unordered_map<uint32_t, std::unordered_map<std::string, std::unique_ptr<service_state>>> service::state;
