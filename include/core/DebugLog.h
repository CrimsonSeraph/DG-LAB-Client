/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "DebugLog_utils.hpp"

#include <QFile>

#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <string>

// 日志等级枚举
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
    LogSinkCallback callback; ///< 回调函数
    LogLevel min_level;       ///< 最小输出等级
};

// DebugLog - 日志系统核心类（单例）
class DebugLog {
public:
    // 单例
    static DebugLog& instance();

    // 禁止拷贝
    DebugLog(const DebugLog&) = delete;
    DebugLog& operator=(const DebugLog&) = delete;

    // 日志等级控制
    /// @brief 设置所有模块的日志等级（枚举版本）
    void set_all_log_level(LogLevel level);

    /// @brief 设置所有模块的日志等级（整数版本: 0=DEBUG,1=INFO,2=WARN,3=ERROR,4=NONE）
    void set_all_log_level(int level);

    /// @brief 设置是否仅输出与模块等级匹配的日志（类型信息模式）
    void set_only_type_info(bool only_type_info);

    /// @brief 设置指定模块的日志等级
    void set_log_level(const std::string& module, LogLevel level);

    /// @brief 获取指定模块的日志等级
    LogLevel get_log_level(const std::string& module) const;

    /// @brief 设置默认日志等级（未被单独设置的模块使用）
    void set_default_log_level(LogLevel level);

    // 日志输出
    /// @brief 核心日志输出函数
    void log(const std::string& module, const std::string& method,
        LogLevel level, const std::string& message);

    // Sink 管理
    /// @brief 注册日志输出通道（Sink）
    void register_log_sink(const std::string& name, const LogSink& sink);

    /// @brief 注销日志输出通道
    void unregister_log_sink(const std::string& name);

    /// @brief 设置某个 Sink 的最小日志等级
    /// @return 成功返回 true，Sink 不存在或等级无效返回 false
    bool set_log_sink_level(const std::string& name, LogLevel level);

    // 文件 Sink
    /// @brief 启用文件日志：写入 dir 下按天切分的 dglab-YYYYMMDD.log
    /// @param dir 日志目录（不存在则创建）
    /// @param level 写入等级下限，低于该等级的日志不落盘
    /// @param max_mb 单个文件大小上限（MB，小于 1 按 1 处理），超出滚动为 *.1
    /// @return 启用成功返回 true，目录或文件无法创建返回 false
    bool enable_file_sink(const std::string& dir, LogLevel level, int max_mb);

    /// @brief 关闭文件日志：注销文件通道并刷新、关闭文件句柄
    void disable_file_sink();

    // 工具函数
    /// @brief 将日志等级枚举转换为字符串
    const char* level_to_string(LogLevel level);

    /// @brief 将整数转换为日志等级枚举
    static LogLevel int_to_log_level(int level);

    /// @brief 检查是否处于“仅类型信息”模式
    inline bool is_only_type_info() const { return is_only_type_info_; }

private:
    DebugLog() = default;

    /// @brief 析构时刷新并关闭文件日志
    ~DebugLog();

    // 成员变量
    mutable std::mutex mutex_;                          ///< 保护 module_log_levels_ 等
    std::map<std::string, LogLevel> module_log_levels_; ///< 各模块日志等级
    LogLevel default_log_level_ = LOG_DEBUG;            ///< 默认日志等级
    bool is_only_type_info_ = false;                    ///< 仅输出类型信息模式

    std::map<std::string, LogSink> log_sinks_; ///< 注册的 Sink
    mutable std::mutex sinks_mutex_;           ///< 保护 log_sinks_

    mutable std::mutex file_mutex_;             ///< 保护文件日志状态与写入
    QFile file_;                                ///< 按天切分的日志文件
    QString file_dir_;                          ///< 日志目录
    QString file_date_;                         ///< 当前文件对应的日期（yyyyMMDD）
    qint64 file_max_bytes_ = 5LL * 1024 * 1024; ///< 单个文件大小上限（字节）
    qint64 file_bytes_ = 0;                     ///< 当前文件已写入字节数
    bool file_enabled_ = false;                 ///< 是否启用文件日志
    bool file_failed_ = false;                  ///< 写入失败后是否已降级为不写文件

    /// @brief 打开指定日期的日志文件（调用方需持有 file_mutex_）
    /// @param date_tag 日期标签（yyyyMMDD）
    /// @param error 失败原因（成功时不变）
    bool open_file_locked(const QString& date_tag, std::string& error);

    /// @brief 将当前文件滚动为 *.1 备份并重新打开（调用方需持有 file_mutex_）
    /// @param error 失败原因（成功时不变）
    bool rotate_file_locked(std::string& error);

    /// @brief 记录一次失败原因并降级为不写文件（调用方需持有 file_mutex_）
    void report_file_failure_locked(const std::string& reason);

    /// @brief 写入一行日志，按需切换日期文件、滚动并处理失败降级
    void write_file_line(const std::string& line);
};

// 日志宏（便捷调用）
#define LOG_MODULE(module, method, level, ...)                                                                       \
    do {                                                                                                             \
        LogLevel moduleLevel = DebugLog::instance().get_log_level(module);                                           \
        bool shouldLog = DebugLog::instance().is_only_type_info() ? (level == moduleLevel) : (level >= moduleLevel); \
        if (shouldLog) {                                                                                             \
            std::ostringstream oss;                                                                                  \
            oss << __VA_ARGS__;                                                                                      \
            DebugLog::instance().log(module, method, level, oss.str());                                              \
        }                                                                                                            \
    } while (0)
