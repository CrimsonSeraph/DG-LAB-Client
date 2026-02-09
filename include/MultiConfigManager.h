#pragma once

#include "ConfigManager.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>

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

private:
    MultiConfigManager() = default;
    ~MultiConfigManager() = default;

    struct ConfigInfo {
        std::string file_path;
        std::shared_ptr<ConfigManager> manager;
        bool auto_reload;
        std::time_t last_mod_time;
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
