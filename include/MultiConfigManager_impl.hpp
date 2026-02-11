#pragma once

#include "MultiConfigManager.h"
#include <sstream>

template<typename T>
std::optional<T> MultiConfigManager::get_with_priority(const std::string& key_path) const {
    if (key_path == "__priority") {
        // 特殊处理，不返回优先级字段本身
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(registry_mutex_);

    try {
        // 获取按优先级排序的配置
        auto sorted_configs = get_sorted_configs();

        // 从低优先级到高优先级查找（高优先级覆盖低优先级）
        std::optional<T> result;

        for (const auto& config : sorted_configs) {
            auto value = config->get<T>(key_path);
            if (value.has_value()) {
                result = value;  // 高优先级覆盖低优先级
                std::cout << "[" << key_path << "] 从优先级 "
                    << config->get<int>("__priority").value_or(0)
                    << " 的配置中获取值" << std::endl;
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
bool MultiConfigManager::set_with_priority(const std::string& key_path,
    const T& value,
    int target_priority) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

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
