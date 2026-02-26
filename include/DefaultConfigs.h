#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace DefaultConfigs {
    nlohmann::json get_default_config(const std::string& config_name);
}
