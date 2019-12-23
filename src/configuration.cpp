#include "configuration.h"
#include "configuration.hpp"
#include "environment.h"

register_service<configuration, configuration_impl> register_configuration;
register_test_service<configuration, configuration_test_impl> register_test_configuration;
