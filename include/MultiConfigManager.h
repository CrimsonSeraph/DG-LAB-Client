#pragma once

#include "ConfigManager.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ============================================
// MultiConfigManager - 多配置管理器（单例）
// ============================================
class MultiConfigManager {
public:
    // -------------------- 单例 --------------------
    /// @brief 获取全局唯一实例
    static MultiConfigManager& instance() {
        static MultiConfigManager instance;
        return instance;
    }

    // -------------------- 公共接口 --------------------
    /// @brief 注册配置文件（延迟加载）
    /// @param name 配置名称（唯一标识）
    /// @param file_path 文件路径
    /// @param auto_reload 是否自动热重载
    void register_config(const std::string& name,
        const std::string& file_path,
        bool auto_reload = false);

    /// @brief 获取指定名称的配置管理器
    /// @param name 配置名称
    /// @return 配置管理器指针（若未注册则抛出异常）
    std::shared_ptr<ConfigManager> get_config(const std::string& name);

    // -------------------- 模板方法（取值/设值）--------------------
    /// @brief 获取配置值（按优先级合并，返回最高优先级的有效值）
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @return 值的 optional，若不存在则 nullopt
    template<typename T>
    std::optional<T> get(const std::string& key_path) const;

    /// @brief 获取配置值（不加锁，需外部同步）
    template<typename T>
    std::optional<T> get_unsafe(const std::string& key_path) const;

    /// @brief 获取指定配置名称中的值
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param key_name 配置名称
    template<typename T>
    std::optional<T> get_with_name(const std::string& key_path, const std::string& key_name) const;

    /// @brief 获取指定配置名称中的值（不加锁）
    template<typename T>
    std::optional<T> get_with_name_unsafe(const std::string& key_path, const std::string& key_name) const;

    /// @brief 设置配置值到指定优先级的配置管理器
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param value 值
    /// @param target_priority 目标优先级，-1 表示最高优先级
    /// @return 成功返回 true
    template<typename T>
    bool set_with_priority(const std::string& key_path,
        const T& value, int target_priority = -1);

    /// @brief 设置配置值到指定优先级的配置管理器（不加锁）
    template<typename T>
    bool set_with_priority_unsafe(const std::string& key_path,
        const T& value, int target_priority);

    /// @brief 设置配置值到指定名称的配置管理器
    /// @tparam T 值类型
    /// @param key_path 键路径
    /// @param value 值
    /// @param key_name 配置名称
    template<typename T>
    bool set_with_name(const std::string& key_path,
        const T& value, const std::string& key_name);

    /// @brief 设置配置值到指定名称的配置管理器（不加锁）
    template<typename T>
    bool set_with_name_unsafe(const std::string& key_path,
        const T& value, const std::string& key_name);

    // -------------------- 批量操作 --------------------
    /// @brief 加载所有已注册的配置
    /// @return 全部成功返回 true，若有失败或优先级冲突返回 false
    bool load_all();

    /// @brief 保存所有配置到文件
    /// @return 全部成功返回 true
    bool save_all();

    /// @brief 重新加载指定配置
    /// @param name 配置名称
    /// @return 成功返回 true
    bool reload(const std::string& name);

    // -------------------- 热重载 --------------------
    /// @brief 启用/禁用文件热重载（监控文件变化自动重载）
    void enable_hot_reload(bool enabled);

    // -------------------- 查询 --------------------
    /// @brief 获取所有已注册配置名称
    std::vector<std::string> get_config_names() const;

    /// @brief 获取按优先级排序的配置管理器列表（加锁）
    std::vector<std::shared_ptr<ConfigManager>> get_sorted_configs() const;

    /// @brief 获取按优先级排序的配置管理器列表（不加锁，需外部同步）
    std::vector<std::shared_ptr<ConfigManager>> get_sorted_configs_unsafe() const;

    // -------------------- 优先级冲突检查 --------------------
    /// @brief 检查是否存在优先级冲突（加锁）
    /// @param error_msg 输出冲突详情
    /// @return 存在冲突返回 true
    bool has_priority_conflict(std::string& error_msg) const;

    /// @brief 检查是否存在优先级冲突（不加锁）
    bool has_priority_conflict_unsafe(std::string& error_msg) const;

    // -------------------- 辅助访问 --------------------
    /// @brief 获取注册表互斥锁（用于高级同步）
    inline std::mutex& get_registry_mutex() { return registry_mutex_; }

    ~MultiConfigManager();

private:
    // 禁止外部构造（单例）
    MultiConfigManager() = default;

    // 友元声明（允许 unique_ptr 删除）
    friend struct std::default_delete<MultiConfigManager>;

    // -------------------- 成员变量 --------------------
    struct ConfigInfo {
        std::string file_path;
        std::shared_ptr<ConfigManager> manager;
        bool auto_reload;
        std::time_t last_mod_time;
        int priority = 0;
    };

    std::unordered_map<std::string, ConfigInfo> config_registry_;   ///< 配置注册表
    mutable std::mutex registry_mutex_; ///< 注册表互斥锁
    bool hot_reload_enabled_ = false;   ///< 热重载开关

    mutable std::vector<std::shared_ptr<ConfigManager>> sorted_configs_cache_;  ///< 排序缓存
    mutable bool cache_dirty_ = true;   ///< 缓存是否失效

    std::thread file_watcher_thread_;   ///< 文件监控线程
    std::atomic<bool> running_{ false };    ///< 监控线程运行标志

    // -------------------- 私有辅助函数 --------------------
    void start_file_watcher();  ///< 启动文件监控线程
    void stop_file_watcher();   ///< 停止文件监控线程
    void file_watcher_loop();   ///< 文件监控循环
    std::time_t get_file_mod_time(const std::string& path) const;   ///< 获取文件修改时间
};

#include "MultiConfigManager_impl.hpp"
