/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "LogExporter.h"

#include "AppConfig.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>

namespace {
// 解析日志行中的级别标签并按过滤条件判断是否导出（手动导出逐行过滤用）
bool should_export_line(const QString& line, int filter_level, bool only_level, bool level_above) {
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
    if (only_level) {
        return value == filter_level;
    }
    return level_above ? (value >= filter_level) : (value <= filter_level);
}
} // namespace

// ============================================
// 构造/析构（public）
// ============================================

LogExporter::LogExporter() = default;

LogExporter::~LogExporter() {
    // 兜底停止自动日志（关闭文件并注销输出通道）
    stop_auto_log();
}

// ============================================
// 设置管理（public）
// ============================================

void LogExporter::load_settings() {
    auto& config = AppConfig::instance();
    // 自动日志设置（兼容旧版平铺键 app.log.export_*）
    auto_settings_.level = config.get_value<int>("app.log.auto.export_level",
        config.get_value<int>("app.log.export_level", 0));
    auto_settings_.only_level = config.get_value<bool>("app.log.auto.export_only_level",
        config.get_value<bool>("app.log.export_only_level", false));
    auto_settings_.level_above = config.get_value<bool>("app.log.auto.export_level_above",
        config.get_value<bool>("app.log.export_level_above", true));
    auto_settings_.dir = config.get_value<std::string>("app.log.auto.export_dir",
        config.get_value<std::string>("app.log.export_dir", "./log"));
    auto_settings_.retain_count = config.get_value<int>("app.log.auto.export_retain_count",
        config.get_value<int>("app.log.export_retain_count", 1));
    auto_settings_.max_size = config.get_value<qint64>("app.log.auto.export_max_size",
        config.get_value<qint64>("app.log.export_max_size", 5LL * 1024 * 1024));
    if (auto_settings_.retain_count < 1) {
        auto_settings_.retain_count = 1;
    }
    // 手动日志设置
    manual_settings_.level = config.get_value<int>("app.log.manual.export_level", 0);
    manual_settings_.only_level = config.get_value<bool>("app.log.manual.export_only_level", false);
    manual_settings_.level_above = config.get_value<bool>("app.log.manual.export_level_above", true);
    manual_settings_.dir = config.get_value<std::string>("app.log.manual.export_dir", "./log/handle");
    LOG_MODULE("LogExporter", "load_settings", LOG_DEBUG,
        "日志设置加载完成: auto[level=" << auto_settings_.level
        << ", only=" << auto_settings_.only_level
        << ", above=" << auto_settings_.level_above
        << ", dir=" << auto_settings_.dir
        << ", retain=" << auto_settings_.retain_count
        << ", max=" << auto_settings_.max_size
        << "] manual[level=" << manual_settings_.level
        << ", only=" << manual_settings_.only_level
        << ", above=" << manual_settings_.level_above
        << ", dir=" << manual_settings_.dir << "]");
}

void LogExporter::save_settings() const {
    auto& config = AppConfig::instance();
    config.set_value_with_name<int>("app.log.auto.export_level", auto_settings_.level, "user");
    config.set_value_with_name<bool>("app.log.auto.export_only_level", auto_settings_.only_level, "user");
    config.set_value_with_name<bool>("app.log.auto.export_level_above", auto_settings_.level_above, "user");
    config.set_value_with_name<std::string>("app.log.auto.export_dir", auto_settings_.dir, "user");
    config.set_value_with_name<int>("app.log.auto.export_retain_count", auto_settings_.retain_count, "user");
    config.set_value_with_name<qint64>("app.log.auto.export_max_size", auto_settings_.max_size, "user");
    config.set_value_with_name<int>("app.log.manual.export_level", manual_settings_.level, "user");
    config.set_value_with_name<bool>("app.log.manual.export_only_level", manual_settings_.only_level, "user");
    config.set_value_with_name<bool>("app.log.manual.export_level_above", manual_settings_.level_above, "user");
    config.set_value_with_name<std::string>("app.log.manual.export_dir", manual_settings_.dir, "user");
    LOG_MODULE("LogExporter", "save_settings", LOG_INFO, "日志设置已保存到 user.json");
}

// ============================================
// 自动日志（public）
// ============================================

void LogExporter::start_auto_log() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (auto_log_active_) {
        return;
    }
    QString dir = auto_dir_absolute();
    if (!ensure_dir(dir)) {
        LOG_MODULE("LogExporter", "start_auto_log", LOG_ERROR,
            "自动日志目录创建失败: " << dir.toStdString());
        return;
    }
    auto_log_timestamp_ = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    auto_log_part_ = 1;
    auto_log_size_ = 0;
    if (!auto_log_file_.isOpen()) {
        auto_log_file_.setFileName(log_file_path(dir, auto_log_timestamp_, 1));
        if (!auto_log_file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            LOG_MODULE("LogExporter", "start_auto_log", LOG_ERROR,
                "自动日志文件打开失败: " << auto_log_file_.fileName().toStdString());
            return;
        }
    }
    auto_log_active_ = true;
    // 注册日志输出通道：所有 LOG_MODULE 输出自动写入文件
    LogSink sink;
    sink.min_level = LOG_DEBUG;
    sink.callback = [this](const std::string& module, const std::string& method,
        LogLevel level, const std::string& message) {
        append_auto_log(module, method, level, message);
    };
    DebugLog::instance().register_log_sink("log_auto_file", sink);
    LOG_MODULE("LogExporter", "start_auto_log", LOG_INFO,
        "自动日志已启动: " << auto_log_file_.fileName().toStdString());
}

void LogExporter::stop_auto_log() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!auto_log_active_) {
        return;
    }
    auto_log_active_ = false;
    if (auto_log_file_.isOpen()) {
        auto_log_file_.flush();
        auto_log_file_.close();
    }
    DebugLog::instance().unregister_log_sink("log_auto_file");
    LOG_MODULE("LogExporter", "stop_auto_log", LOG_INFO, "自动日志已停止");
}

// ============================================
// 手动导出与清理（public）
// ============================================

bool LogExporter::export_log(const QString& content, QString* error) {
    // 手动日志目录（默认程序目录下 log/handle/）
    QString dir = manual_dir_absolute();
    if (!ensure_dir(dir)) {
        if (error) *error = "手动日志目录创建失败: " + dir;
        LOG_MODULE("LogExporter", "export_log", LOG_ERROR,
            "手动日志目录创建失败: " << dir.toStdString());
        return false;
    }

    // 按手动设置过滤日志级别
    QString filtered;
    const QStringList lines = content.split('\n');
    for (const QString& line : lines) {
        if (should_export_line(line, manual_settings_.level, manual_settings_.only_level,
                manual_settings_.level_above)) {
            filtered += line + "\n";
        }
    }
    if (filtered.isEmpty()) {
        if (error) *error = "没有满足导出条件的日志内容";
        LOG_MODULE("LogExporter", "export_log", LOG_WARN, "没有满足导出条件的日志内容");
        return false;
    }

    // 手动日志不受数量/大小限制：单文件写入（每次导出独立时间戳文件）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QFile file(log_file_path(dir, timestamp, 1));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = "日志文件写入失败: " + file.fileName();
        LOG_MODULE("LogExporter", "export_log", LOG_ERROR,
            "手动日志文件写入失败: " << file.fileName().toStdString());
        return false;
    }
    file.write(filtered.toUtf8());
    file.close();
    LOG_MODULE("LogExporter", "export_log", LOG_INFO,
        "手动日志导出成功: " << file.fileName().toStdString());
    return true;
}

void LogExporter::cleanup_old_logs() {
    QDir dir(auto_dir_absolute());
    if (!dir.exists()) {
        return;
    }
    QStringList files = dir.entryList({"log_*.txt"}, QDir::Files);
    if (files.isEmpty()) {
        return;
    }
    // 按分组键分组（log_yyyyMMdd_HHmmss[_分片号].txt，分片视为一份）
    QMap<QString, QStringList> groups;
    QRegularExpression group_re("^(log_\\d{8}_\\d{6})(?:_\\d+)?\\.txt$");
    for (const QString& file : files) {
        QRegularExpressionMatch match = group_re.match(file);
        QString key = match.hasMatch() ? match.captured(1) : file;
        groups[key].append(file);
    }
    // 组按时间戳字典序排序（旧在前）
    QStringList keys = groups.keys();
    std::sort(keys.begin(), keys.end());
    // 仅保留最新 retain_count 份（分片整组保留/删除）
    int remove_count = static_cast<int>(keys.size()) - auto_settings_.retain_count;
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

QString LogExporter::auto_dir_absolute() const {
    QString dir = QString::fromStdString(auto_settings_.dir);
    QDir d(dir);
    if (d.isAbsolute()) {
        return QDir::cleanPath(dir);
    }
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/" + dir);
}

QString LogExporter::manual_dir_absolute() const {
    QString dir = QString::fromStdString(manual_settings_.dir);
    QDir d(dir);
    if (d.isAbsolute()) {
        return QDir::cleanPath(dir);
    }
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/" + dir);
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

bool LogExporter::should_log_level(LogLevel level, int filter_level, bool only_level,
    bool level_above) {
    int value = static_cast<int>(level);
    if (only_level) {
        return value == filter_level;
    }
    return level_above ? (value >= filter_level) : (value <= filter_level);
}

void LogExporter::append_auto_log(const std::string& module, const std::string& method,
    LogLevel level, const std::string& message) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!auto_log_active_ || !auto_log_file_.isOpen()) {
        return;
    }
    // 按自动设置过滤日志级别
    if (!should_log_level(level, auto_settings_.level, auto_settings_.only_level,
            auto_settings_.level_above)) {
        return;
    }
    QString line = QString("[%1] <%2> (%3): %4\n")
            .arg(QString::fromStdString(module))
            .arg(QString::fromStdString(method))
            .arg(QString::fromUtf8(DebugLog::instance().level_to_string(level)))
            .arg(QString::fromStdString(message));
    QByteArray line_bytes = line.toUtf8();
    // 超过单个文件大小上限则轮转分片（按字节计算）
    if (auto_log_size_ + line_bytes.size() > auto_settings_.max_size && auto_log_size_ > 0) {
        rotate_auto_log_file();
    }
    qint64 written = auto_log_file_.write(line_bytes);
    if (written > 0) {
        auto_log_size_ += written;
    }
    // 立即落盘，避免异常退出（强杀/崩溃）丢失日志
    auto_log_file_.flush();
}

void LogExporter::rotate_auto_log_file() {
    if (auto_log_file_.isOpen()) {
        auto_log_file_.flush();
        auto_log_file_.close();
    }
    ++auto_log_part_;
    auto_log_size_ = 0;
    auto_log_file_.setFileName(log_file_path(auto_dir_absolute(), auto_log_timestamp_, auto_log_part_));
    if (!auto_log_file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        LOG_MODULE("LogExporter", "rotate_auto_log_file", LOG_ERROR,
            "自动日志分片文件打开失败: " << auto_log_file_.fileName().toStdString());
    }
}

bool LogExporter::ensure_dir(const QString& dir) {
    QDir d(dir);
    return d.exists() || d.mkpath(".");
}

QString LogExporter::log_file_path(const QString& dir, const QString& timestamp, int part_index) {
    QString filename = (part_index <= 1)
        ? QString("log_%1.txt").arg(timestamp)
        : QString("log_%1_%2.txt").arg(timestamp).arg(part_index);
    return QDir(dir).filePath(filename);
}
