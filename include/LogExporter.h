/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>

#include <string>

// ============================================
// LogExporter - 日志导出器
// 维护导出设置（级别过滤、保留数量、大小上限、导出位置），
// 负责导出界面日志到文件（超限分片）与清理多余日志
// ============================================
class LogExporter {
public:
    // -------------------- 导出设置结构 --------------------
    /// @brief 日志导出设置（持久化到 user.json 的 app.log 下）
    struct Settings {
        int level = 0;                            ///< 导出日志级别（LOG_DEBUG=0/INFO=1/WARN=2/ERROR=3）
        bool only_level = false;                  ///< 是否只导出指定级别
        bool level_above = true;                  ///< 指定级别及以上(true)/以下(false)
        std::string dir = "./log";                ///< 导出位置（相对程序目录，如 "./log"）
        int retain_count = 1;                     ///< 保留日志数量（分片日志视为一份）
        qint64 max_size = 5 * 1024 * 1024;        ///< 单个日志大小上限（字节），超出分片写入多个文件
    };

    // -------------------- 构造/析构 --------------------
    LogExporter() = default;

    // -------------------- 设置管理 --------------------
    /// @brief 从配置（user.json 的 app.log）加载导出设置
    void load_settings();

    /// @brief 将当前导出设置保存到配置（user.json 的 app.log）
    void save_settings() const;

    /// @brief 设置导出设置
    /// @param settings 新的导出设置
    inline void set_settings(const Settings& settings) { settings_ = settings; }

    /// @brief 获取当前导出设置
    /// @return 导出设置引用
    inline const Settings& settings() const { return settings_; }

    // -------------------- 导出与清理 --------------------
    /// @brief 导出日志内容到文件（按设置过滤级别，超限分片写入）
    /// @param content 日志全文（如界面日志控件内容）
    /// @param error 输出错误信息（可选）
    /// @return 成功返回 true
    bool export_log(const QString& content, QString* error = nullptr);

    /// @brief 清理多余日志：目录中仅保留最新 retain_count 份（分片日志视为一份）
    void cleanup_old_logs();

    /// @brief 获取导出目录绝对路径（相对路径基于程序目录解析）
    /// @return 绝对路径
    QString export_dir_absolute() const;

private:
    // -------------------- 成员变量 --------------------
    Settings settings_; ///< 导出设置

    // -------------------- 私有辅助函数 --------------------
    /// @brief 判断日志行是否满足导出过滤条件
    /// @param line 日志行文本
    /// @return 满足条件返回 true
    bool should_export_line(const QString& line) const;

    /// @brief 写入单个日志分片文件
    /// @param dir 导出目录绝对路径
    /// @param timestamp 时间戳
    /// @param part_index 分片序号（1 起始，1 时不带序号后缀）
    /// @param content 分片内容
    /// @return 成功返回 true
    static bool write_log_part(const QString& dir, const QString& timestamp, int part_index,
        const QString& content);
};
