#pragma once

#include "DebugLog.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
    bool update(const nlohmann::json& patch);   // 合并更新
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

#include "ConfigManager_impl.hpp"
