#pragma once

#include "AppConfig.h"
#include "DebugLog.h"

#include <mutex>
#include <string>
#include <vector>

// ============================================
// 模板方法实现
// ============================================

template<typename... Args>
inline void AppConfig::batch_update(Args&&... updates) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 使用折叠表达式应用所有更新
    (std::forward<Args>(updates)(*this), ...);

    // 保存配置
    save_all();
}

template<typename T>
inline T AppConfig::get_value(const std::string& key_path, T default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_value_unsafe<T>(key_path, default_value);
}

template<typename T>
inline T AppConfig::get_value_unsafe(const std::string& key_path, T default_value) const {
    if (!multi_config_) {
        LOG_MODULE("AppConfig", "get_value_unsafe", LOG_ERROR, "配置系统未初始化，返回默认值: " << key_path);
        return default_value;
    }

    auto value = multi_config_->get<T>(key_path);
    return value.has_value() ? value.value() : default_value;
}

template<typename T>
inline void AppConfig::set_value_with_priority(const std::string& key_path,
    const T& value, int target_priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_value_with_priority_unsafe<T>(key_path, value, target_priority);
}

template<typename T>
inline void AppConfig::set_value_with_priority_unsafe(const std::string& key_path,
    const T& value, int target_priority) {
    if (!multi_config_) {
        throw std::runtime_error("配置系统未初始化");
    }

    bool success = multi_config_->set_with_priority(key_path, value, target_priority);
    if (!success) {
        LOG_MODULE("AppConfig", "set_value_with_priority_unsafe", LOG_WARN, "设置配置失败: " << key_path
            << " (目标优先级: " << target_priority << ")");
    }
}

template<typename T>
inline T AppConfig::get_value_with_name(const std::string& key_path,
    T default_value, const std::string& key_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_value_with_name_unsafe<T>(key_path, default_value, key_name);
}

template<typename T>
inline T AppConfig::get_value_with_name_unsafe(const std::string& key_path,
    T default_value, const std::string& key_name) const {
    if (!multi_config_) {
        LOG_MODULE("AppConfig", "get_value_with_name_unsafe", LOG_ERROR, "配置系统未初始化，返回默认值: " << key_path);
        return default_value;
    }

    auto value = multi_config_->get_with_name<T>(key_path, key_name);
    return value.has_value() ? value.value() : default_value;
}

template<typename T>
inline void AppConfig::set_value_with_name(const std::string& key_path,
    const T& value, const std::string& key_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_value_with_name_unsafe<T>(key_path, value, key_name);
}

template<typename T>
inline void AppConfig::set_value_with_name_unsafe(const std::string& key_path,
    const T& value, const std::string& key_name) {
    if (!multi_config_) {
        throw std::runtime_error("配置系统未初始化");
    }

    bool success = multi_config_->set_with_name(key_path, value, key_name);
    if (!success) {
        LOG_MODULE("AppConfig", "set_value_with_name_unsafe", LOG_WARN, "设置配置失败: " << key_path
            << " (目标名称: " << key_name << ")");
    }
}

template<typename T>
inline void AppConfig::set_value(const std::string& key_path, const T& value) {
    LOG_MODULE("AppConfig", "set_value", LOG_INFO, "设置配置值: " << key_path << " = " << value);
    set_value_with_priority<T>(key_path, value, -1);
}
