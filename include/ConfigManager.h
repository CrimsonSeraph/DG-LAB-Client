#pragma once

#include "DebugLog.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <optional>

class ConfigManager {
private:
    nlohmann::json config_;
    std::string config_path_;
    mutable std::recursive_mutex mutex_;
    bool loaded_ = false;

    // 配置变更监听器
    std::vector<std::function<void(const nlohmann::json&)>> observers_;

public:
    // 禁止拷贝，允许移动
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = default;
    ConfigManager& operator=(ConfigManager&&) = default;

    explicit ConfigManager(const std::string& path = "config.json");
    virtual ~ConfigManager() = default;

    // 加载和保存
    bool load();
    bool save() const;

    // 获取配置值
    template<typename T>
    std::optional<T> get(const std::string& key_path) const;

    template<typename T>
    T get(const std::string& key_path, T default_value) const;

    // 设置配置值
    template<typename T>
    bool set(const std::string& key_path, const T& value);

    // 批量操作
    bool update(const nlohmann::json& patch);  // 合并更新
    bool remove(const std::string& key_path);

    // 监听器
    void add_listener(std::function<void(const nlohmann::json&)> listener);

    // 获取原始json（只读）
    inline const nlohmann::json& raw() const { return config_; }

    // 验证配置
    virtual bool validate() const;

protected:
    virtual nlohmann::json get_default_config() const;

private:
    std::vector<std::string> split_key_path(const std::string& key_path) const;
    mutable std::unordered_map<std::string, std::vector<std::string>> split_cache_;
    void notify_listeners() const;
};

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

        return current.get<T>();

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
        auto keys = split_key_path(key_path);
        nlohmann::json* current = &config_;

        // 导航到指定路径（创建不存在的中间节点）
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->contains(keys[i])) {
                (*current)[keys[i]] = nlohmann::json::object();
            }
            current = &(*current)[keys[i]];
        }

        // 设置最终值
        (*current)[keys.back()] = value;

        return true;

    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "set", LOG_ERROR, "设置配置失败 [" << key_path << "]: " << e.what());
        return false;
    }
}
