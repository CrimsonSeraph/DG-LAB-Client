#pragma once

#include "ConfigManager.h"
#include "DebugLog.h"

#include <mutex>
#include <vector>

// ============================================
// 模板函数实现
// ============================================

template<typename T>
inline std::optional<T> ConfigManager::get(const std::string& key_path) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        auto keys = split_key_path(key_path);
        nlohmann::json current = config_;

        for (const auto& key : keys) {
            if (!current.contains(key)) {
                return std::nullopt;
            }
            current = current.at(key);
        }

        T val = current.get<T>();
        LOG_MODULE("ConfigManager", "get", LOG_DEBUG,
            "获取配置成功 [" << key_path << "] = " << nlohmann::json(val).dump());
        return val;

    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "get", LOG_ERROR, "获取配置失败 [" << key_path << "]: " << e.what());
        return std::nullopt;
    }
}

template<typename T>
inline T ConfigManager::get(const std::string& key_path, T default_value) const {
    auto value = get<T>(key_path);
    return value.has_value() ? value.value() : default_value;
}

template<typename T>
inline bool ConfigManager::set(const std::string& key_path, const T& value) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        nlohmann::json new_config = config_;
        auto keys = split_key_path(key_path);
        nlohmann::json* current = &new_config;

        // 导航到指定路径（创建不存在的中间节点）
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->contains(keys[i])) {
                (*current)[keys[i]] = nlohmann::json::object();
            }
            current = &(*current)[keys[i]];
        }

        // 设置最终值
        (*current)[keys.back()] = value;
        std::swap(config_, new_config);
        LOG_MODULE("ConfigManager", "set", LOG_DEBUG,
            "设置配置成功 [" << key_path << "] = " << nlohmann::json(value).dump());
        return true;

    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "set", LOG_ERROR, "设置配置失败 [" << key_path << "]: " << e.what());
        return false;
    }
    catch (...) {
        LOG_MODULE("ConfigManager", "set", LOG_ERROR, "设置配置失败 [" << key_path << "]: 未知异常");
        return false;
    }
}
