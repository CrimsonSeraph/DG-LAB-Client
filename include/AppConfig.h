#pragma once

#include "AppConfig_utils.hpp"
#include "ConfigStructs.h"
#include "DebugLog.h"
#include "MultiConfigManager.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// 前置声明
class MultiConfigManager;
class ConfigManager;

// ============================================
// AppConfig - 应用配置主类（单例）
// ============================================
class AppConfig {
public:
    // -------------------- 单例 --------------------
    /// @brief 获取单例实例
    static AppConfig& instance();

    // 禁止拷贝和赋值
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    // -------------------- 初始化与关闭 --------------------
    /// @brief 初始化配置系统
    /// @param config_dir 配置目录路径，默认为 "./config"
    /// @return 成功返回 true，失败返回 false（将使用内存默认配置）
    bool initialize(const std::string& config_dir = "./config");

    /// @brief 关闭配置系统，保存所有配置并释放资源
    void shutdown();

    /// @brief 检查配置系统是否已初始化
    /// @return 已初始化返回 true
    bool is_initialized() const;

    /// @brief 检查配置文件优先级是否存在冲突
    /// @param error_msg 输出冲突详细信息
    /// @return 存在冲突返回 true
    bool check_priority_conflict(std::string& error_msg) const;

    /// @brief 线程安全的初始化状态检查（内联）
    inline bool is_initialized_thread_safe() const { return initialized_; }

    // -------------------- 模板方法（取值/设值） --------------------
    /// @brief 设置配置值（写入最高优先级配置）
    /// @tparam T 值类型
    /// @param key_path 键路径，如 "app.name"
    /// @param value 要设置的值
    template<typename T>
    void set_value(const std::string& key_path, const T& value);

    /// @brief 批量更新多个配置
    /// @tparam Args 更新函数类型
    /// @param updates 更新函数（接受 AppConfig& 参数的 lambda）
    template<typename... Args>
    void batch_update(Args&&... updates);

    /// @brief 获取配置值（线程安全，带缓存）
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param default_value 默认值（当配置不存在时返回）
    /// @return 配置值或默认值
    template<typename T>
    T get_value(const std::string& key_path, T default_value) const;

    /// @brief 获取配置值（不加锁，需外部同步）
    /// @tparam T 值类型
    template<typename T>
    T get_value_unsafe(const std::string& key_path, T default_value) const;

    /// @brief 设置配置值并指定目标优先级
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param value 值
    /// @param target_priority 目标优先级（数字越大优先级越高）
    template<typename T>
    void set_value_with_priority(const std::string& key_path, const T& value, int target_priority);

    /// @brief 设置配置值并指定目标优先级（不加锁）
    template<typename T>
    void set_value_with_priority_unsafe(const std::string& key_path, const T& value, int target_priority);

    /// @brief 获取指定配置文件中的值
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param default_value 默认值
    /// @param key_name 配置文件名（如 "main"）
    template<typename T>
    T get_value_with_name(const std::string& key_path, T default_value, const std::string& key_name) const;

    /// @brief 获取指定配置文件中的值（不加锁）
    template<typename T>
    T get_value_with_name_unsafe(const std::string& key_path, T default_value, const std::string& key_name) const;

    /// @brief 设置指定配置文件中的值
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param value 值
    /// @param key_name 配置文件名
    template<typename T>
    void set_value_with_name(const std::string& key_path, const T& value, const std::string& key_name);

    /// @brief 设置指定配置文件中的值（不加锁）
    template<typename T>
    void set_value_with_name_unsafe(const std::string& key_path, const T& value, const std::string& key_name);

    // -------------------- 批量操作 --------------------
    /// @brief 保存所有配置到文件
    /// @return 全部保存成功返回 true
    bool save_all();

    /// @brief 重新加载所有配置文件
    void reload_all();

    // -------------------- 配置监听 --------------------
    /// @brief 添加配置变更监听器
    /// @param config_name 配置名称（"main"/"user"/"system"/"all"）
    /// @param listener 回调函数
    void add_config_listener(const std::string& config_name, std::function<void()> listener);

    /// @brief 移除配置变更监听器（基于函数指针比较，不完全可靠）
    /// @param config_name 配置名称
    /// @param listener 要移除的回调函数
    void remove_config_listener(const std::string& config_name, std::function<void()> listener);

    // -------------------- 配置验证 --------------------
    /// @brief 验证所有配置项
    /// @param errors 输出错误列表
    /// @return 全部有效返回 true
    bool validate_all(std::vector<std::string>& errors) const;

    /// @brief 验证特定配置项
    /// @param config_name 配置名称
    /// @param errors 输出错误列表
    /// @return 有效返回 true
    bool validate_config(const std::string& config_name, std::vector<std::string>& errors) const;

    // -------------------- 高级操作 --------------------
    /// @brief 获取原始配置管理器
    /// @param name 配置名称
    /// @return 配置管理器指针，可能为空
    std::shared_ptr<ConfigManager> get_config_manager(const std::string& name);

    /// @brief 获取所有已注册配置名称
    /// @return 配置名称列表
    std::vector<std::string> get_all_config_names() const;

    /// @brief 检查配置是否存在
    bool has_config(const std::string& name) const;

    /// @brief 导出配置到文件
    /// @param name 配置名称
    /// @param file_path 目标文件路径
    /// @return 成功返回 true
    bool export_config(const std::string& name, const std::string& file_path) const;

    /// @brief 从文件导入配置
    /// @param name 配置名称
    /// @param file_path 源文件路径
    /// @return 成功返回 true
    bool import_config(const std::string& name, const std::string& file_path);

    // -------------------- 配置对象访问器 --------------------
    inline const ConfigObject<MainConfig>& main_config() const { return main_config_obj_; }
    inline const ConfigObject<SystemConfig>& system_config() const { return system_config_obj_; }
    inline const ConfigObject<UserConfig>& user_config() const { return user_config_obj_; }

private:
    // -------------------- 构造/析构（单例私有）--------------------
    AppConfig();
    ~AppConfig();

    // -------------------- 成员变量 --------------------
    MultiConfigManager* multi_config_;                      ///< 多配置管理器指针
    std::shared_ptr<ConfigManager> main_config_;            ///< 主配置管理器
    std::shared_ptr<ConfigManager> user_config_;            ///< 用户配置管理器
    std::shared_ptr<ConfigManager> system_config_;          ///< 系统配置管理器

    ConfigObject<MainConfig> main_config_obj_;              ///< 主配置对象
    ConfigObject<SystemConfig> system_config_obj_;          ///< 系统配置对象
    ConfigObject<UserConfig> user_config_obj_;              ///< 用户配置对象

    std::map<std::string, std::vector<std::function<void()>>> config_listeners_; ///< 配置监听器映射
    mutable std::mutex mutex_;                              ///< 互斥锁
    std::atomic<bool> initialized_{ false };                ///< 初始化标志

    // -------------------- 私有辅助函数 --------------------
    void initialize_configs();                              ///< 初始化配置项（加锁）
    void initialize_configs_unsafe();                       ///< 初始化配置项（不加锁）
    void create_default_configs();                          ///< 创建默认配置文件
    void setup_listeners();                                 ///< 设置配置变更监听
    void invalidate_caches();                               ///< 清空所有配置缓存
    void notify_config_changed(const std::string& config_name); ///< 通知配置变更
    bool validate_configs() const;                          ///< 验证所有配置（内部调用）
    bool is_called_from_main_thread() const;                ///< 检查是否在主线程
};

#include "AppConfig_impl.hpp"
