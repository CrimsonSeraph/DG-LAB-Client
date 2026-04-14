#pragma once

#include "DebugLog_utils.hpp"

#include <functional>
#include <map>
#include <mutex>
#include <string>

// ============================================
// 日志等级枚举
// ============================================
enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_NONE = 4
};

using LogSinkCallback = std::function<void(const std::string& module,
    const std::string& method,
    LogLevel level,
    const std::string& message)>;

struct LogSink {
    LogSinkCallback callback;   ///< 回调函数
    LogLevel min_level;         ///< 最小输出等级
};

// ============================================
// DebugLog - 日志系统核心类（单例）
// ============================================
class DebugLog {
public:
    // -------------------- 单例 --------------------
    static DebugLog& instance();

    // 禁止拷贝
    DebugLog(const DebugLog&) = delete;
    DebugLog& operator=(const DebugLog&) = delete;

    // -------------------- 日志等级控制 --------------------
    /// @brief 设置所有模块的日志等级（枚举版本）
    void set_all_log_level(LogLevel level);

    /// @brief 设置所有模块的日志等级（整数版本：0=DEBUG,1=INFO,2=WARN,3=ERROR,4=NONE）
    void set_all_log_level(int level);

    /// @brief 设置是否仅输出与模块等级匹配的日志（类型信息模式）
    void set_only_type_info(bool only_type_info);

    /// @brief 设置指定模块的日志等级
    void set_log_level(const std::string& module, LogLevel level);

    /// @brief 获取指定模块的日志等级
    LogLevel get_log_level(const std::string& module) const;

    /// @brief 设置默认日志等级（未被单独设置的模块使用）
    void set_default_log_level(LogLevel level);

    // -------------------- 日志输出 --------------------
    /// @brief 核心日志输出函数
    void log(const std::string& module, const std::string& method,
        LogLevel level, const std::string& message);

    // -------------------- Sink 管理 --------------------
    /// @brief 注册日志输出通道（Sink）
    void register_log_sink(const std::string& name, const LogSink& sink);

    /// @brief 注销日志输出通道
    void unregister_log_sink(const std::string& name);

    /// @brief 设置某个 Sink 的最小日志等级
    /// @return 成功返回 true，Sink 不存在或等级无效返回 false
    bool set_log_sink_level(const std::string& name, LogLevel level);

    // -------------------- 工具函数 --------------------
    /// @brief 将日志等级枚举转换为字符串
    const char* level_to_string(LogLevel level);

    /// @brief 将整数转换为日志等级枚举
    static LogLevel int_to_log_level(int level);

    /// @brief 检查是否处于“仅类型信息”模式
    inline bool is_only_type_info() const { return is_only_type_info_; }

private:
    DebugLog() = default;
    ~DebugLog() = default;

    // -------------------- 成员变量 --------------------
    mutable std::mutex mutex_;                              ///< 保护 module_log_levels_ 等
    std::map<std::string, LogLevel> module_log_levels_;    ///< 各模块日志等级
    LogLevel default_log_level_ = LOG_DEBUG;               ///< 默认日志等级
    bool is_only_type_info_ = false;                       ///< 仅输出类型信息模式

    std::map<std::string, LogSink> log_sinks_;              ///< 注册的 Sink
    mutable std::mutex sinks_mutex_;                       ///< 保护 log_sinks_
};

// ============================================
// 日志宏（便捷调用）
// ============================================
#define LOG_MODULE(module, method, level, ...) \
    do { \
        LogLevel moduleLevel = DebugLog::instance().get_log_level(module); \
        bool shouldLog = DebugLog::instance().is_only_type_info() ? (level == moduleLevel) : (level >= moduleLevel); \
        if (shouldLog) { \
            std::ostringstream oss; \
            oss << __VA_ARGS__; \
            DebugLog::instance().log(module, method, level, oss.str()); \
        } \
    } while (0)
