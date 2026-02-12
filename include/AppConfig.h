#pragma once

#include "AppConfig_impl.hpp"

#include "MultiConfigManager.h"
#include "ConfigStructs.h"
#include <iostream>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>

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
    bool is_initialized_thread_safe() const { return initialized_; }

    // ================ 配置项访问器 ================

    // 应用信息
    const std::string& get_app_name() const;
    const std::string& get_app_name_unsafe() const;

    const std::string& get_app_version() const;
    bool is_debug_mode() const;
    int get_log_level() const;

    // ================ 配置项修改器 ================

    // 应用设置
    void set_app_name(const std::string& name);
    void set_debug_mode(bool enabled);
    void set_log_level(int level);
    void set_value(const std::string& key_path, const std::string& value);

    // 应用设置（通过优先级）
    void set_app_name_with_priority(const std::string& name, int priority);
    void set_debug_mode_with_priority(const bool enable, int priority);
    void set_log_level_with_priority(int level, int priority);
    void set_value_with_priority(const std::string& key_path, const std::string& value, int priority);

    // 应用设置（通过名称）
    void set_app_name_with_name(const std::string& name, const std::string& key_name);
    void set_debug_mode_with_name(const bool enable, const std::string& key_name);
    void set_log_level_with_name(int level, const std::string& key_name);
    void set_value_with_name(const std::string& key_path, const std::string& value, const std::string& key_name);

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
    ConfigValue<std::string> app_name_;
    ConfigValue<std::string> app_version_;
    ConfigValue<bool> debug_mode_;
    ConfigValue<int> log_level_;

    // ConfigObject<XXXConfig> db_config_;

    // 监听器
    std::map<std::string, std::vector<std::function<void()>>> config_listeners_;

    // 互斥锁
    mutable std::mutex mutex_;

    // 初始化标志
    bool initialized_ = false;
};


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

// 获取配置
template<typename T>
T AppConfig::get_value(const std::string& key_path, T default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_value_unsafe<T>(key_path, default_value);
}

template<typename T>
T AppConfig::get_value_unsafe(const std::string& key_path, T default_value) const {
    if (!multi_config_) {
        std::cerr << "配置系统未初始化，返回默认值: " << key_path << std::endl;
        return default_value;
    }

    auto value = multi_config_->get<T>(key_path);
    return value.has_value() ? value.value() : default_value;
}

// 设置配置（带优先级）
template<typename T>
void AppConfig::set_value_with_priority(const std::string& key_path,
    const T& value, int target_priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_value_with_priority_unsafe<T>(key_path, value, target_priority);
}

template<typename T>
void AppConfig::set_value_with_priority_unsafe(const std::string& key_path,
    const T& value, int target_priority) {
    if (!multi_config_) {
        throw std::runtime_error("配置系统未初始化");
    }

    bool success = multi_config_->set_with_priority(key_path, value, target_priority);
    if (!success) {
        std::cerr << "设置配置失败: " << key_path
            << " (目标优先级: " << target_priority << ")" << std::endl;
    }
}

// 获取配置（带名称）
template<typename T>
T AppConfig::get_value_with_name(const std::string& key_path,
    T default_value, const std::string& key_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_value_with_name_unsafe<T>(key_path, default_value, key_path);
}

template<typename T>
T AppConfig::get_value_with_name_unsafe(const std::string& key_path,
    T default_value, const std::string& key_name) const {
    if (!multi_config_) {
        std::cerr << "配置系统未初始化，返回默认值: " << key_path << std::endl;
        return default_value;
    }

    auto value = multi_config_->get_with_name<T>(key_path, key_name);
    return value.has_value() ? value.value() : default_value;
}

// 设置配置（带名称）
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
        std::cerr << "设置配置失败: " << key_path
            << " (目标名称: " << key_name << ")" << std::endl;
    }
}

// ============================================
// 辅助函数和宏
// ============================================

// 配置访问宏（简化访问）
#define CONFIG_GET(instance, config) (instance).get_##config()
#define CONFIG_SET(instance, config, value) (instance).set_##config(value)
#define CONFIG_UPDATE(instance, config, value) (instance).update_##config(value)

// 配置验证宏
#define VALIDATE_CONFIG(config) \
    if (!(config).validate()) { \
        throw std::runtime_error("配置验证失败: " #config); \
    }

// 配置监听宏
#define ADD_CONFIG_LISTENER(instance, config_name, callback) \
    (instance).add_config_listener(config_name, callback)

// 配置构建辅助
#define BEGIN_CONFIG_BUILDER(type) ConfigBuilder<type> builder
#define ADD_CONFIG_FIELD(field, value) builder.set_field(field, value)
#define BUILD_CONFIG builder.build()
