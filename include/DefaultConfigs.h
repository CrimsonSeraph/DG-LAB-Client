#pragma once

#include <nlohmann/json.hpp>
#include <string>

// ============================================
// DefaultConfigs - 默认配置工厂命名空间
// ============================================
namespace DefaultConfigs {

    /// @brief 根据配置名称获取对应的默认配置 JSON
    /// @param config_name 配置名称（"main" / "system" / "user" / 其他）
    /// @return 默认配置的 JSON 对象
    nlohmann::json get_default_config(const std::string& config_name);

}   // namespace DefaultConfigs
