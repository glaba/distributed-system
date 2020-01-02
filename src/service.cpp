#include "service.h"

std::recursive_mutex service::state_mutex;
std::unordered_map<uint32_t, std::unordered_map<std::string, service::state_info>> service::state;
