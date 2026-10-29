/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "DebugLog.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// 配置文件监听状态（定义在 ConfigManager.cpp）
struct ConfigWatchState;

// ConfigManager - 单配置文件管理器
class ConfigManager {
public:
    // 构造/析构
    /// @brief 禁止拷贝
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /// @brief 禁止移动
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

    /// @brief 构造函数
    /// @param path 配置文件路径，默认为 "config.json"
    explicit ConfigManager(const std::string& path = "config.json");
    virtual ~ConfigManager();

    // 加载、重读与保存
    /// @brief 加载配置文件（若文件不存在则创建默认配置；已加载时幂等返回，不重读磁盘）
    /// @return 成功返回 true，失败返回 false（将使用内存默认配置）
    bool load();

    /// @brief 从磁盘重读并整体替换内存配置，成功后通知监听器
    /// @return 成功返回 true；文件缺失、JSON 损坏或校验不通过时返回 false 并保留上一份有效配置
    bool reload();

    /// @brief 保存当前配置到文件
    /// @return 成功返回 true
    /// @note 写盘前与磁盘做三方合并，外部新增的键不会被内存数据覆盖
    bool save();

    /// @brief 开启/关闭配置文件变更监听
    /// @param enabled 开启为 true
    /// @note 监听仍在同一个实例内：去抖后调用 reload()，通知沿用既有 listener
    void watch_changes(bool enabled);

    /// @brief 是否已开启配置文件变更监听
    /// @return 已开启返回 true
    bool watching_changes() const;

    // 模板方法（取值/设值）
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

    // 批量操作
    /// @brief 合并更新配置（使用 JSON Patch 语义）
    /// @param patch 要合并的 JSON 对象
    /// @return 成功返回 true
    bool update(const nlohmann::json& patch);

    /// @brief 删除指定键路径的配置项
    /// @param key_path 键路径
    /// @return 删除成功返回 true，键不存在返回 false
    bool remove(const std::string& key_path);

    // 监听器
    /// @brief 添加配置变更监听器
    /// @param listener 回调函数，参数为变更后的完整配置 JSON
    void add_listener(std::function<void(const nlohmann::json&)> listener);

    // 原始访问
    /// @brief 获取原始 JSON 配置（只读）
    inline const nlohmann::json& raw() const { return config_; }

    // 验证
    /// @brief 验证配置有效性（可被子类重写）
    /// @return 有效返回 true
    virtual bool validate() const;

protected:
    /// @brief 获取默认配置（可被子类重写）
    /// @return 默认配置的 JSON 对象
    virtual nlohmann::json get_default_config() const;

    /// @brief 校验候选配置（重读时先验证后替换，避免半成品配置进入内存）
    /// @param candidate 待校验的完整配置
    /// @return 有效返回 true
    bool validate_json(const nlohmann::json& candidate) const;

private:
    // 磁盘读取结果
    enum class DiskReadResult {
        kOk,      ///< 读取并解析成功
        kMissing, ///< 文件不存在
        kInvalid  ///< 读取失败或 JSON 损坏
    };

    // 成员变量
    nlohmann::json config_;              ///< 配置数据
    std::string config_path_;            ///< 配置文件路径
    mutable std::recursive_mutex mutex_; ///< 递归互斥锁
    bool loaded_ = false;                ///< 是否已加载

    std::vector<std::function<void(const nlohmann::json&)>> observers_; ///< 监听器列表

    mutable std::unordered_map<std::string, std::vector<std::string>> split_cache_; ///< 键路径拆分缓存

    std::unique_ptr<ConfigWatchState> watch_;                     ///< 文件监听状态（未开启时为空）
    mutable std::string synced_content_;                          ///< 上次与磁盘同步的内容（三方合并基准）
    mutable std::string self_write_content_;                      ///< 最近一次自身写盘的内容
    mutable std::chrono::steady_clock::time_point self_write_at_{}; ///< 最近一次自身写盘的时刻
    mutable bool has_self_write_ = false;                         ///< 是否存在待判定的自身写盘事件

    // 私有辅助函数
    /// @brief 拆分键路径（如 "a.b.c" -> ["a","b","c"]），使用缓存
    /// @param key_path 键路径
    /// @return 拆分后的字符串向量
    /// @note 调用此函数前必须已持有 mutex_
    std::vector<std::string> split_key_path(const std::string& key_path) const;

    /// @brief 通知所有监听器配置已变更
    void notify_listeners() const;

    /// @brief 读取并解析磁盘配置文件
    /// @param raw 输出原始文本
    /// @param parsed 输出解析结果
    /// @return 读取结果分类
    DiskReadResult read_disk_config(std::string& raw, nlohmann::json& parsed) const;

    /// @brief 记录与磁盘已同步的内容（读盘成功后调用，同时清掉自写标记）
    /// @param raw 磁盘原始文本
    void remember_synced_content(const std::string& raw) const;

    /// @brief 记录一次自身写盘（内容与时刻），供监听事件判定是否为自触发
    /// @param payload 写盘的完整文本
    void mark_self_write(const std::string& payload) const;

    /// @brief 取走「本次事件来自自身写盘」的判定
    /// @return 是自身写盘返回 true
    bool consume_self_write_event() const;

    /// @brief 保存前与磁盘做三方合并，把外部改动并入内存
    void align_with_disk_before_save();

    /// @brief 磁盘内容是否与上次同步内容一致（目录级事件会带来同目录其它文件的写盘）
    /// @return 一致返回 true
    bool disk_content_matches_synced() const;

    /// @brief 去抖到点后的处理：自身写盘忽略，内容未变跳过，否则重读（失败保留旧值）
    void handle_debounced_event();
};

#include "ConfigManager_impl.hpp"
