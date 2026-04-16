/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "DebugLog.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================
// ConfigManager - 单配置文件管理器
// ============================================
class ConfigManager {
public:
    // -------------------- 构造/析构 --------------------
    /// @brief 禁止拷贝
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /// @brief 允许移动
    ConfigManager(ConfigManager&&) = default;
    ConfigManager& operator=(ConfigManager&&) = default;

    /// @brief 构造函数
    /// @param path 配置文件路径，默认为 "config.json"
    explicit ConfigManager(const std::string& path = "config.json");
    virtual ~ConfigManager() = default;

    // -------------------- 加载与保存 --------------------
    /// @brief 加载配置文件（若文件不存在则创建默认配置）
    /// @return 成功返回 true，失败返回 false（将使用内存默认配置）
    bool load();

    /// @brief 保存当前配置到文件
    /// @return 成功返回 true
    bool save() const;

    // -------------------- 模板方法（取值/设值）--------------------
    /// @brief 获取配置值（返回 optional）
    /// @tparam T 值类型
    /// @param key_path 键路径，如 "app.name"
    /// @return 存在则返回值的 optional，否则 nullopt
    template<typename T>
    std::optional<T> get(const std::string& key_path) const;

    /// @brief 获取配置值（带默认值）
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param default_value 默认值
    /// @return 配置值或默认值
    template<typename T>
    T get(const std::string& key_path, T default_value) const;

    /// @brief 设置配置值（自动创建中间节点）
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param value 值
    /// @return 成功返回 true
    template<typename T>
    bool set(const std::string& key_path, const T& value);

    // -------------------- 批量操作 --------------------
    /// @brief 合并更新配置（使用 JSON Patch 语义）
    /// @param patch 要合并的 JSON 对象
    /// @return 成功返回 true
    bool update(const nlohmann::json& patch);

    /// @brief 删除指定键路径的配置项
    /// @param key_path 键路径
    /// @return 删除成功返回 true，键不存在返回 false
    bool remove(const std::string& key_path);

    // -------------------- 监听器 --------------------
    /// @brief 添加配置变更监听器
    /// @param listener 回调函数，参数为变更后的完整配置 JSON
    void add_listener(std::function<void(const nlohmann::json&)> listener);

    // -------------------- 原始访问 --------------------
    /// @brief 获取原始 JSON 配置（只读）
    inline const nlohmann::json& raw() const { return config_; }

    // -------------------- 验证 --------------------
    /// @brief 验证配置有效性（可被子类重写）
    /// @return 有效返回 true
    virtual bool validate() const;

protected:
    /// @brief 获取默认配置（可被子类重写）
    /// @return 默认配置的 JSON 对象
    virtual nlohmann::json get_default_config() const;

private:
    // -------------------- 成员变量 --------------------
    nlohmann::json config_; ///< 配置数据
    std::string config_path_;   ///< 配置文件路径
    mutable std::recursive_mutex mutex_;    ///< 递归互斥锁
    bool loaded_ = false;   ///< 是否已加载

    std::vector<std::function<void(const nlohmann::json&)>> observers_; ///< 监听器列表

    mutable std::unordered_map<std::string, std::vector<std::string>> split_cache_; ///< 键路径拆分缓存

    // -------------------- 私有辅助函数 --------------------
    /// @brief 拆分键路径（如 "a.b.c" -> ["a","b","c"]），使用缓存
    /// @param key_path 键路径
    /// @return 拆分后的字符串向量
    /// @note 调用此函数前必须已持有 mutex_
    std::vector<std::string> split_key_path(const std::string& key_path) const;

    /// @brief 通知所有监听器配置已变更
    void notify_listeners() const;
};

#include "ConfigManager_impl.hpp"
