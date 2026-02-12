#pragma once

#include "ConfigManager.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <functional>

class MultiConfigManager {
public:
    // 单例模式（全局唯一配置管理器）
    static MultiConfigManager& instance() {
        static MultiConfigManager instance;
        return instance;
    }

    // 注册配置（延迟加载）
    void register_config(const std::string& name,
        const std::string& file_path,
        bool auto_reload = false);

    // 获取配置管理器
    std::shared_ptr<ConfigManager> get_config(const std::string& name);

    // 获取配置值
    template<typename T>
    std::optional<T> get(const std::string& key_path) const;
    template<typename T>
    std::optional<T> get_unsafe(const std::string& key_path) const;

    // 获取配置值（通过名称）
    template<typename T>
    std::optional<T> get_with_name(const std::string& key_path, const std::string& key_name) const;
    template<typename T>
    std::optional<T> get_with_name_unsafe(const std::string& key_path, const std::string& key_name) const;

    // 检查优先级冲突
    bool has_priority_conflict_unsafe(std::string& error_msg) const;
    bool has_priority_conflict(std::string& error_msg) const;

    // 设置配置值（指定优先级）
    template<typename T>
    bool set_with_priority(const std::string& key_path,
        const T& value, int target_priority = -1);

    template<typename T>
    bool set_with_priority_unsafe(const std::string& key_path,
        const T& value, int target_priority);

    // 设置配置值（指定名称）
    template<typename T>
    bool set_with_name(const std::string& key_path,
        const T& value, const std::string& key_name);

    template<typename T>
    bool set_with_name_unsafe(const std::string& key_path,
        const T& value, const std::string& key_name);

    // 加载所有配置
    bool load_all();

    // 保存所有配置
    bool save_all();

    // 重新加载特定配置
    bool reload(const std::string& name);

    // 热重载：监控文件变化
    void enable_hot_reload(bool enabled);

    // 获取所有配置名称
    std::vector<std::string> get_config_names() const;

    // 获取互斥锁（用于线程同步）
    std::mutex& get_registry_mutex() { return registry_mutex_; }

    // 获取排序后的配置管理器
    std::vector<std::shared_ptr<ConfigManager>> get_sorted_configs_unsafe() const;
    std::vector<std::shared_ptr<ConfigManager>> get_sorted_configs() const;

    ~MultiConfigManager() = default;

private:
    MultiConfigManager() = default;

    // 允许 make_unique 访问
    friend struct std::default_delete<MultiConfigManager>;

    struct ConfigInfo {
        std::string file_path;
        std::shared_ptr<ConfigManager> manager;
        bool auto_reload;
        std::time_t last_mod_time;
        int priority = 0;
    };

    std::unordered_map<std::string, ConfigInfo> config_registry_;
    mutable std::mutex registry_mutex_;
    bool hot_reload_enabled_ = false;

    // 文件监控线程
    std::thread file_watcher_thread_;
    std::atomic<bool> running_{ false };

    void start_file_watcher();
    void stop_file_watcher();
    void file_watcher_loop();
    std::time_t get_file_mod_time(const std::string& path) const;
};

#include "MultiConfigManager_impl.hpp"
