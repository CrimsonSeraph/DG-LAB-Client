/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "LogExporter.h"

#include "AppConfig.h"
#include "DebugLog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>

// ============================================
// 设置管理（public）
// ============================================

void LogExporter::load_settings() {
    auto& config = AppConfig::instance();
    settings_.level = config.get_value<int>("app.log.export_level", 0);
    settings_.only_level = config.get_value<bool>("app.log.export_only_level", false);
    settings_.level_above = config.get_value<bool>("app.log.export_level_above", true);
    settings_.dir = config.get_value<std::string>("app.log.export_dir", "./log");
    settings_.retain_count = config.get_value<int>("app.log.export_retain_count", 1);
    settings_.max_size = config.get_value<qint64>("app.log.export_max_size", 5LL * 1024 * 1024);
    if (settings_.retain_count < 1) {
        settings_.retain_count = 1;
    }
    LOG_MODULE("LogExporter", "load_settings", LOG_DEBUG,
        "导出设置加载完成: level=" << settings_.level
        << ", only=" << settings_.only_level
        << ", above=" << settings_.level_above
        << ", dir=" << settings_.dir
        << ", retain=" << settings_.retain_count
        << ", max_size=" << settings_.max_size);
}

void LogExporter::save_settings() const {
    auto& config = AppConfig::instance();
    config.set_value_with_name<int>("app.log.export_level", settings_.level, "user");
    config.set_value_with_name<bool>("app.log.export_only_level", settings_.only_level, "user");
    config.set_value_with_name<bool>("app.log.export_level_above", settings_.level_above, "user");
    config.set_value_with_name<std::string>("app.log.export_dir", settings_.dir, "user");
    config.set_value_with_name<int>("app.log.export_retain_count", settings_.retain_count, "user");
    config.set_value_with_name<qint64>("app.log.export_max_size", settings_.max_size, "user");
    LOG_MODULE("LogExporter", "save_settings", LOG_INFO, "导出设置已保存到 user.json");
}

// ============================================
// 导出与清理（public）
// ============================================

bool LogExporter::export_log(const QString& content, QString* error) {
    // 确保导出目录存在；不可用则回退默认目录并警告
    QString dir = export_dir_absolute();
    QDir qdir(dir);
    if (!qdir.exists() && !qdir.mkpath(".")) {
        LOG_MODULE("LogExporter", "export_log", LOG_WARN,
            "导出目录不可用，回退默认目录: " << dir.toStdString());
        settings_.dir = "./log";
        dir = export_dir_absolute();
        qdir = QDir(dir);
        if (!qdir.exists() && !qdir.mkpath(".")) {
            if (error) *error = "导出目录创建失败: " + dir;
            LOG_MODULE("LogExporter", "export_log", LOG_ERROR,
                "导出目录创建失败: " << dir.toStdString());
            return false;
        }
    }

    // 按导出设置过滤日志级别
    QString filtered;
    const QStringList lines = content.split('\n');
    for (const QString& line : lines) {
        if (should_export_line(line)) {
            filtered += line + "\n";
        }
    }
    if (filtered.isEmpty()) {
        if (error) *error = "没有满足导出条件的日志内容";
        LOG_MODULE("LogExporter", "export_log", LOG_WARN, "没有满足导出条件的日志内容");
        return false;
    }

    // 时间戳命名：log_yyyyMMdd_HHmmss.txt（分片追加 _N）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    qint64 max_size = settings_.max_size > 0 ? settings_.max_size : (5LL * 1024 * 1024);

    // 内容未超限：单文件写入
    if (filtered.size() <= max_size) {
        if (!write_log_part(dir, timestamp, 1, filtered)) {
            if (error) *error = "日志文件写入失败";
            return false;
        }
    }
    else {
        // 超限分片：按行边界切分，每片不超过大小上限
        int part_index = 1;
        QString part;
        const QStringList filtered_lines = filtered.split('\n');
        for (const QString& line : filtered_lines) {
            if (!part.isEmpty() && part.size() + line.size() + 1 > max_size) {
                if (!write_log_part(dir, timestamp, part_index, part)) {
                    if (error) *error = "日志文件写入失败（分片 " + QString::number(part_index) + "）";
                    return false;
                }
                ++part_index;
                part.clear();
            }
            part += line + "\n";
        }
        if (!part.isEmpty()) {
            if (!write_log_part(dir, timestamp, part_index, part)) {
                if (error) *error = "日志文件写入失败（分片 " + QString::number(part_index) + "）";
                return false;
            }
        }
    }
    LOG_MODULE("LogExporter", "export_log", LOG_INFO,
        "日志导出成功，位置: " << dir.toStdString() << "，时间戳: " << timestamp.toStdString());
    // 导出后立即清理多余日志（保留最新 N 份，分片视为一份），程序退出时也会兜底清理
    cleanup_old_logs();
    return true;
}

void LogExporter::cleanup_old_logs() {
    QDir dir(export_dir_absolute());
    if (!dir.exists()) {
        return;
    }
    QStringList files = dir.entryList({"log_*.txt"}, QDir::Files);
    if (files.isEmpty()) {
        return;
    }
    // 按分组键分组（log_<时间戳>[_<分片号>].txt，分片视为一份）
    QMap<QString, QStringList> groups;
    QRegularExpression group_re("^(log_\\d{8}_\\d{6})(?:_\\d+)?\\.txt$");
    for (const QString& file : files) {
        QRegularExpressionMatch match = group_re.match(file);
        QString key = match.hasMatch() ? match.captured(1) : file;
        groups[key].append(file);
    }
    // 组按时间戳字典序排序（yyyyMMdd_HHmmss 字典序即时间序，旧在前）
    QStringList keys = groups.keys();
    std::sort(keys.begin(), keys.end());
    // 仅保留最新 retain_count 份（分片整组保留/删除）
    int remove_count = static_cast<int>(keys.size()) - settings_.retain_count;
    for (int i = 0; i < remove_count; ++i) {
        for (const QString& file : groups[keys[i]]) {
            QString path = dir.filePath(file);
            if (QFile::remove(path)) {
                LOG_MODULE("LogExporter", "cleanup_old_logs", LOG_DEBUG,
                    "已清理旧日志: " << file.toStdString());
            }
        }
    }
}

QString LogExporter::export_dir_absolute() const {
    QString dir = QString::fromStdString(settings_.dir);
    QDir d(dir);
    if (d.isAbsolute()) {
        return QDir::cleanPath(dir);
    }
    // 相对路径基于程序目录解析（如 "./log" -> 程序目录/log）
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/" + dir);
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

bool LogExporter::should_export_line(const QString& line) const {
    // 解析日志行中的级别标签（如 "(DEBUG)"）
    static const QRegularExpression level_re("\\((DEBUG|INFO|WARN|ERROR|NONE)\\)");
    QRegularExpressionMatch match = level_re.match(line);
    if (!match.hasMatch()) {
        // 无级别标签的行（空行/标题等）默认保留
        return true;
    }
    QString level = match.captured(1);
    int value = (level == "DEBUG") ? 0
        : (level == "INFO") ? 1
        : (level == "WARN") ? 2
        : (level == "ERROR") ? 3
        : 4;
    if (settings_.only_level) {
        // 仅导出指定级别
        return value == settings_.level;
    }
    // 指定级别及以上（数值更大=更严重）或以下
    return settings_.level_above ? (value >= settings_.level) : (value <= settings_.level);
}

bool LogExporter::write_log_part(const QString& dir, const QString& timestamp, int part_index,
    const QString& content) {
    QString filename = (part_index <= 1)
        ? QString("log_%1.txt").arg(timestamp)
        : QString("log_%1_%2.txt").arg(timestamp).arg(part_index);
    QFile file(QDir(dir).filePath(filename));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_MODULE("LogExporter", "write_log_part", LOG_ERROR,
            "日志文件打开失败: " << filename.toStdString());
        return false;
    }
    file.write(content.toUtf8());
    file.close();
    LOG_MODULE("LogExporter", "write_log_part", LOG_DEBUG,
        "日志分片写入完成: " << filename.toStdString() << "，大小: " << content.size());
    return true;
}
