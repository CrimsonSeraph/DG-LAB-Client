#pragma once

#include "MultiConfigManager.h"
#include <sstream>

template<typename T>
std::optional<T> MultiConfigManager::get(const std::string& key_path) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return get_unsafe<T>(key_path);
}

template<typename T>
inline std::optional<T> MultiConfigManager::get_unsafe(const std::string& key_path) const {
    if (key_path == "__priority") {
        // 特殊处理，不返回优先级字段本身
        return std::nullopt;
    }
    try {
        // 获取按优先级排序的配置
        auto sorted_configs = get_sorted_configs_unsafe();

        // 从低优先级到高优先级查找（高优先级覆盖低优先级）
        std::optional<T> result;
        for (const auto& config : sorted_configs) {
            try {
                auto value = config->get<T>(key_path);
                if (value.has_value()) {
                    result = value;  // 高优先级覆盖
                }
            }
            catch (const std::exception& e) {
                std::cerr << "配置 [" << key_path << "] 读取失败（优先级 "
                    << config->get<int>("__priority").value_or(0) << "）: "
                    << e.what() << std::endl;
            }
        }
        return result;
    }
    catch (const std::exception& e) {
        std::cerr << "按优先级获取配置失败 [" << key_path << "]: "
            << e.what() << std::endl;
        return std::nullopt;
    }
}

template<typename T>
std::optional<T> MultiConfigManager::get_with_name(const std::string& key_path, const std::string& key_name) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return get_with_name_unsafe<T>(key_path, key_name);
}

template<typename T>
inline std::optional<T> MultiConfigManager::get_with_name_unsafe(const std::string& key_path, const std::string& key_name) const {
    if (key_path == "__priority") {
        // 特殊处理，不返回优先级字段本身
        return std::nullopt;
    }
    try {
        std::optional<T> result;

        for (auto& [name, info] : config_registry_) {
            if (info.manager && name == key_name) {
                auto value = info.manager->get<T>(key_path);
                if (value.has_value()) {
                    return value;
                }
                std::cerr << "名称为 " << key_name << " 的配置管理器没有名为["
                    << key_path << "]" << std::endl;
            }
        }

        std::cerr << "未找到名称为 " << key_name << " 的配置管理器" << std::endl;
        return std::nullopt;
    }
    catch (const std::exception& e) {
        std::cerr << "按名称获取配置失败 [" << key_path << "]: "
            << "[" << key_name << "]"
            << e.what() << std::endl;
        return std::nullopt;
    }
}

template<typename T>
bool MultiConfigManager::set_with_priority(const std::string& key_path,
    const T& value, int target_priority) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return set_with_priority_unsafe<T>(key_path, value, target_priority);
}

template<typename T>
bool MultiConfigManager::set_with_priority_unsafe(const std::string& key_path,
    const T& value, int target_priority) {
    // 如果 target_priority 为 -1，则设置到最高优先级的配置管理器
    if (target_priority == -1) {
        // 找到最高优先级的配置管理器
        auto it = std::max_element(config_registry_.begin(), config_registry_.end(),
            [](const auto& a, const auto& b) {
                return a.second.priority < b.second.priority;
            });
        if (it != config_registry_.end() && it->second.manager) {
            bool success = it->second.manager->set(key_path, value);
            if (success) {
                it->second.manager->save();
                return true;
            }
        }
        return false;
    }

    // 否则，设置到指定优先级的配置管理器
    for (auto& [name, info] : config_registry_) {
        if (info.manager && info.priority == target_priority) {
            bool success = info.manager->set(key_path, value);
            if (success) {
                info.manager->save();
            }
            return success;
        }
    }

    std::cerr << "未找到优先级为 " << target_priority << " 的配置管理器" << std::endl;
    return false;
}

template<typename T>
bool MultiConfigManager::set_with_name(const std::string& key_path,
    const T& value, const std::string& key_name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return set_with_name_unsafe<T>(key_path, value, key_name);
}

template<typename T>
bool MultiConfigManager::set_with_name_unsafe(const std::string& key_path,
    const T& value, const std::string& key_name) {
    for (auto& [name, info] : config_registry_) {
        if (info.manager && name == key_name) {
            bool success = info.manager->set(key_path, value);
            if (success) {
                info.manager->save();
            }
            return success;
        }
    }

    std::cerr << "未找到名称为 " << key_name << " 的配置管理器" << std::endl;
    return false;
}
