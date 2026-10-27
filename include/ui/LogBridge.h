/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "DebugLog.h"
#include "LogExporter.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class QTimer;

// LogBridge - 界面日志桥接（QML 上下文属性 logBridge）
// 职责：注册 DebugLog 输出通道收集运行日志供 QML 展示、按级别过滤、
//      导出界面日志（手动日志），并启动自动日志记录。
class LogBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(int levelFilter READ level_filter NOTIFY levelFilterChanged)
    Q_PROPERTY(int exportLevel READ export_level NOTIFY exportSettingsChanged)
    Q_PROPERTY(int exportRange READ export_range NOTIFY exportSettingsChanged)
    Q_PROPERTY(QString exportDir READ export_dir NOTIFY exportSettingsChanged)
    Q_PROPERTY(QString exportDirConfig READ export_dir_config NOTIFY exportSettingsChanged)
    Q_PROPERTY(QString lastExportPath READ last_export_path NOTIFY exportChanged)

public:
    explicit LogBridge(QObject* parent = nullptr);
    ~LogBridge() override;

    /// @brief 注册日志通道、启动自动日志并读取导出目录
    void initialize();

    QVariantList messages() const { return messages_; }
    int level_filter() const { return level_filter_; }
    /// @brief 手动导出级别（0 DEBUG / 1 INFO / 2 WARN / 3 ERROR）
    int export_level() const;
    /// @brief 手动导出范围（0 仅该级别 / 1 该级别及以上 / 2 该级别及以下）
    int export_range() const;
    QString export_dir() const;
    /// @brief 手动导出目录配置值原文（相对路径原样保留）
    QString export_dir_config() const;
    /// @brief 最近一次成功导出的日志文件完整路径（未导出过为空）
    QString last_export_path() const { return last_export_path_; }

    Q_INVOKABLE void clear();
    Q_INVOKABLE bool exportLogs();
    /// @brief 设置界面日志级别（0 DEBUG / 1 INFO / 2 WARN / 3 ERROR）
    Q_INVOKABLE void setLevelFilter(int level);
    /// @brief 设置手动导出级别（0 DEBUG / 1 INFO / 2 WARN / 3 ERROR）
    Q_INVOKABLE void setExportLevel(int level);
    /// @brief 设置手动导出范围（0 仅该级别 / 1 该级别及以上 / 2 该级别及以下）
    Q_INVOKABLE void setExportRange(int range);
    /// @brief 设置手动导出目录（相对路径基于程序目录解析，目录不存在则创建）
    /// @return 成功返回 true；空路径或创建失败返回 false 并发出 statusMessage
    Q_INVOKABLE bool setExportDir(const QString& dir);

signals:
    void messagesChanged();
    void levelFilterChanged();
    /// @brief 手动导出设置（级别 / 范围 / 目录）变化
    void exportSettingsChanged();
    /// @brief 导出成功（lastExportPath 已更新）
    void exportChanged();
    /// @brief 导出失败原因（成功不发出，成功走 exportChanged）
    void statusMessage(const QString& message);

private:
    void on_log(const std::string& module, const std::string& method, LogLevel level,
        const std::string& message);
    void flush_pending();
    /// @brief 把手动导出设置写入 user.json 并通知界面
    void save_export_settings();

    QVariantList messages_;
    bool dirty_ = false;
    QTimer* flush_timer_ = nullptr;
    int level_filter_ = 0;
    QString last_export_path_;
    LogExporter exporter_;
};
