#pragma once

#include "AppConfig_impl.hpp"
#include "ConfigStructs.h"
#include "DebugLog.h"
#include "MultiConfigManager.h"

#include <atomic>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

// 前置声明
class MultiConfigManager;
class ConfigManager;

// ============================================
// AppConfig - 应用配置主类
// ============================================
class AppConfig {
public:
    // 获取单例实例
    static AppConfig& instance();
    // 禁止拷贝和赋值
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;
    // 初始化配置系统
    bool initialize(const std::string& config_dir = "./config");
    // 销毁配置系统
    void shutdown();
    // 初始化检查
    bool is_initialized() const;
    // 检查优先级冲突
    bool check_priority_conflict(std::string& error_msg) const;
    // 线程安全检查
    inline bool is_initialized_thread_safe() const { return initialized_; }

    // ================ 配置项修改器 ================

    // 应用设置
    template<typename T>
    void set_value(const std::string& key_path, const T& value);

    // ================ 批量操作 ================

    // 批量更新多个配置
    template<typename... Args>
    void batch_update(Args&&... updates);
    // 获取配置
    template<typename T>
    T get_value(const std::string& key_path, T default_value) const;
    template<typename T>
    T get_value_unsafe(const std::string& key_path, T default_value) const;
    // 设置配置（带优先级）
    template<typename T>
    void set_value_with_priority(const std::string& key_path, const T& value, int target_priority);
    template<typename T>
    void set_value_with_priority_unsafe(const std::string& key_path, const T& value, int target_priority);
    // 获取配置（带名称）
    template<typename T>
    T get_value_with_name(const std::string& key_path, T default_value, const std::string& key_name) const;
    template<typename T>
    T get_value_with_name_unsafe(const std::string& key_path, T default_value, const std::string& key_name) const;
    // 设置配置（带名称）
    template<typename T>
    void set_value_with_name(const std::string& key_path, const T& value, const std::string& key_name);
    template<typename T>
    void set_value_with_name_unsafe(const std::string& key_path, const T& value, const std::string& key_name);
    // 保存所有配置
    bool save_all();
    // 重新加载所有配置
    void reload_all();

    // ================ 配置监听 ================

    // 添加配置变更监听器
    void add_config_listener(const std::string& config_name,
        std::function<void()> listener);
    // 移除配置变更监听器
    void remove_config_listener(const std::string& config_name,
        std::function<void()> listener);

    // ================ 配置验证 ================

    // 验证所有配置
    bool validate_all(std::vector<std::string>& errors) const;
    // 验证特定配置
    bool validate_config(const std::string& config_name,
        std::vector<std::string>& errors) const;

    // ================ 高级操作 ================

    // 获取原始配置管理器
    std::shared_ptr<ConfigManager> get_config_manager(const std::string& name);
    // 获取所有配置名称
    std::vector<std::string> get_all_config_names() const;
    // 检查配置是否存在
    bool has_config(const std::string& name) const;
    // 导出配置到文件
    bool export_config(const std::string& name, const std::string& file_path) const;
    // 导入配置从文件
    bool import_config(const std::string& name, const std::string& file_path);

    inline const ConfigObject<MainConfig>& main_config() const { return main_config_obj_; }
    inline const ConfigObject<SystemConfig>& system_config() const { return system_config_obj_; }
    inline const ConfigObject<UserConfig>& user_config() const { return user_config_obj_; }

private:
    // 私有构造函数（单例模式）
    AppConfig();
    ~AppConfig();
    // 初始化配置项
    void initialize_configs();
    void initialize_configs_unsafe();
    // 创建创建默认配置
    void create_default_configs();
    // 设置配置变更监听
    void setup_listeners();
    // 清空所有缓存
    void invalidate_caches();
    // 通知配置变更
    void notify_config_changed(const std::string& config_name);
    // 验证配置
    bool validate_configs() const;
    // 检查是否在主线程
    bool is_called_from_main_thread() const;

    // ================ 成员变量 ================

    // 配置管理器
    MultiConfigManager* multi_config_;
    // 配置管理器引用
    std::shared_ptr<ConfigManager> main_config_;
    std::shared_ptr<ConfigManager> user_config_;
    std::shared_ptr<ConfigManager> system_config_;
    // 配置项
    ConfigObject<MainConfig> main_config_obj_;
    ConfigObject<SystemConfig> system_config_obj_;
    ConfigObject<UserConfig> user_config_obj_;

    // ConfigObject<XXXConfig> db_config_;

    // 监听器
    std::map<std::string, std::vector<std::function<void()>>> config_listeners_;
    // 互斥锁
    mutable std::mutex mutex_;
    // 初始化标志
    std::atomic<bool> initialized_{ false };
};

#include "AppConfig_template.hpp"
