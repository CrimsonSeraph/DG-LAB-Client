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
    Q_PROPERTY(QString exportDir READ export_dir NOTIFY levelFilterChanged)
    Q_PROPERTY(QString lastExportPath READ last_export_path NOTIFY exportChanged)

public:
    explicit LogBridge(QObject* parent = nullptr);
    ~LogBridge() override;

    /// @brief 注册日志通道、启动自动日志并读取导出目录
    void initialize();

    QVariantList messages() const { return messages_; }
    int level_filter() const { return level_filter_; }
    QString export_dir() const;
    /// @brief 最近一次成功导出的日志文件完整路径（未导出过为空）
    QString last_export_path() const { return last_export_path_; }

    Q_INVOKABLE void clear();
    Q_INVOKABLE bool exportLogs();
    /// @brief 设置界面日志级别（0 DEBUG / 1 INFO / 2 WARN / 3 ERROR）
    Q_INVOKABLE void setLevelFilter(int level);

signals:
    void messagesChanged();
    void levelFilterChanged();
    /// @brief 导出成功（lastExportPath 已更新）
    void exportChanged();
    /// @brief 导出失败原因（成功不发出，成功走 exportChanged）
    void statusMessage(const QString& message);

private:
    void on_log(const std::string& module, const std::string& method, LogLevel level,
        const std::string& message);
    void flush_pending();

    QVariantList messages_;
    bool dirty_ = false;
    QTimer* flush_timer_ = nullptr;
    int level_filter_ = 0;
    QString last_export_path_;
    LogExporter exporter_;
};
