/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "DebugLog.h"

#include <QFile>
#include <QString>

#include <functional>
#include <mutex>
#include <string>

// ============================================
// LogExporter - 日志导出器
// 自动日志：程序启动后持续记录运行日志到自动目录（受数量/大小限制，分片轮转）
// 手动日志：点击导出时导出界面日志到手动目录（不受数量/大小限制）
// ============================================
class LogExporter {
public:
    // -------------------- 自动日志设置结构 --------------------
    /// @brief 自动日志设置（持久化到 user.json 的 app.log.auto 下）
    struct AutoSettings {
        int level = 0;                     ///< 导出日志级别（LOG_DEBUG=0/INFO=1/WARN=2/ERROR=3）
        bool only_level = false;           ///< 是否只导出指定级别
        bool level_above = true;           ///< 指定级别及以上(true)/以下(false)
        std::string dir = "./log";         ///< 自动日志目录（相对程序目录）
        int retain_count = 1;              ///< 保留日志数量（分片日志视为一份）
        qint64 max_size = 5 * 1024 * 1024; ///< 单个日志大小上限（字节），超出分片写入多个文件
    };

    // -------------------- 手动日志设置结构 --------------------
    /// @brief 手动日志设置（持久化到 user.json 的 app.log.manual 下）
    struct ManualSettings {
        int level = 0;                     ///< 导出日志级别（LOG_DEBUG=0/INFO=1/WARN=2/ERROR=3）
        bool only_level = false;           ///< 是否只导出指定级别
        bool level_above = true;           ///< 指定级别及以上(true)/以下(false)
        std::string dir = "./log/handle";  ///< 手动日志目录（相对程序目录）
    };

    // -------------------- 构造/析构 --------------------
    LogExporter();
    ~LogExporter();

    // -------------------- 设置管理 --------------------
    /// @brief 从配置（user.json 的 app.log）加载自动/手动日志设置
    void load_settings();

    /// @brief 将自动/手动日志设置保存到配置（user.json 的 app.log）
    void save_settings() const;

    /// @brief 设置自动日志设置
    /// @param settings 自动日志设置
    inline void set_auto_settings(const AutoSettings& settings) { auto_settings_ = settings; }

    /// @brief 设置手动日志设置
    /// @param settings 手动日志设置
    inline void set_manual_settings(const ManualSettings& settings) { manual_settings_ = settings; }

    /// @brief 获取自动日志设置
    /// @return 自动日志设置引用
    inline const AutoSettings& auto_settings() const { return auto_settings_; }

    /// @brief 获取手动日志设置
    /// @return 手动日志设置引用
    inline const ManualSettings& manual_settings() const { return manual_settings_; }

    // -------------------- 自动日志 --------------------
    /// @brief 启动自动日志（注册日志输出通道，程序配置加载完成后调用）
    void start_auto_log();

    /// @brief 停止自动日志（关闭文件并注销输出通道）
    void stop_auto_log();

    /// @brief 自动日志是否运行中
    /// @return 运行中返回 true
    inline bool is_auto_logging() const { return auto_log_active_; }

    // -------------------- 手动导出与清理 --------------------
    /// @brief 手动导出日志内容到手动目录（按手动设置过滤，不受数量/大小限制）
    /// @param content 日志全文（如界面日志控件内容）
    /// @param error 输出错误信息（可选）
    /// @return 成功返回 true
    bool export_log(const QString& content, QString* error = nullptr);

    /// @brief 清理自动目录中多余日志：仅保留最新 retain_count 份（分片日志视为一份）
    void cleanup_old_logs();

    /// @brief 获取自动日志目录绝对路径（相对路径基于程序目录解析）
    /// @return 绝对路径
    QString auto_dir_absolute() const;

    /// @brief 获取手动日志目录绝对路径（相对路径基于程序目录解析）
    /// @return 绝对路径
    QString manual_dir_absolute() const;

private:
    // -------------------- 成员变量 --------------------
    AutoSettings auto_settings_;   ///< 自动日志设置
    ManualSettings manual_settings_; ///< 手动日志设置
    mutable std::recursive_mutex mutex_; ///< 保护自动日志文件写入（持锁内 LOG_MODULE 递归进入 sink 需可重入）

    QFile auto_log_file_;          ///< 自动日志当前文件
    QString auto_log_timestamp_;   ///< 自动日志时间戳（文件命名）
    int auto_log_part_ = 1;        ///< 自动日志分片序号
    qint64 auto_log_size_ = 0;     ///< 自动日志当前文件大小
    bool auto_log_active_ = false; ///< 自动日志运行标志

    // -------------------- 私有辅助函数 --------------------
    /// @brief 判断日志级别是否满足过滤条件
    /// @param level 日志级别
    /// @param filter_level 过滤级别
    /// @param only_level 是否只导出指定级别
    /// @param level_above 指定级别及以上/以下
    /// @return 满足条件返回 true
    static bool should_log_level(LogLevel level, int filter_level, bool only_level,
        bool level_above);

    /// @brief 追加一条日志到自动日志文件（按自动设置过滤，超限轮转分片）
    /// @param module 模块名
    /// @param method 函数名
    /// @param level 日志级别
    /// @param message 日志内容
    void append_auto_log(const std::string& module, const std::string& method, LogLevel level,
        const std::string& message);

    /// @brief 轮转自动日志文件（当前文件超限时开启下一个分片文件）
    void rotate_auto_log_file();

    /// @brief 确保目录存在，不存在则创建
    /// @param dir 目录绝对路径
    /// @return 成功返回 true
    static bool ensure_dir(const QString& dir);

    /// @brief 生成目录下唯一的时间戳文件名
    /// @param dir 目录绝对路径
    /// @param timestamp 时间戳
    /// @param part_index 分片序号（1 起始）
    /// @return 完整文件路径
    static QString log_file_path(const QString& dir, const QString& timestamp, int part_index);
};
