/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "LogBridge.h"

#include "AppConfig.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>

#include <algorithm>

namespace {

    constexpr int kMaxMessages = 400;

    /// @brief 手动日志目录下最近写入的日志文件（LogExporter 每次导出写一个独立时间戳文件）
    QString newest_export_file(const QString& dir) {
        const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("log_*.txt")}, QDir::Files, QDir::Time);
        return files.isEmpty() ? QString() : files.first().absoluteFilePath();
    }

    QString level_text(LogLevel level) {
        switch (level) {
        case LOG_DEBUG: return QStringLiteral("DEBUG");
        case LOG_INFO: return QStringLiteral("INFO");
        case LOG_WARN: return QStringLiteral("WARN");
        case LOG_ERROR: return QStringLiteral("ERROR");
        default: return QStringLiteral("NONE");
        }
    }

} // namespace

LogBridge::LogBridge(QObject* parent)
    : QObject(parent) {
}

LogBridge::~LogBridge() = default;

void LogBridge::initialize() {
    exporter_.load_settings();
    exporter_.start_auto_log();

    flush_timer_ = new QTimer(this);
    flush_timer_->setInterval(200);
    connect(flush_timer_, &QTimer::timeout, this, &LogBridge::flush_pending);
    flush_timer_->start();

    level_filter_ = AppConfig::instance().get_value<int>("app.log.ui_log_level", 0);

    LogSink sink;
    sink.min_level = LOG_DEBUG;
    sink.callback = [this](const std::string& module, const std::string& method, LogLevel level,
                        const std::string& message) { on_log(module, method, level, message); };
    DebugLog::instance().register_log_sink("qt_ui", sink);
    DebugLog::instance().set_log_sink_level("qt_ui", static_cast<LogLevel>(level_filter_));
    emit levelFilterChanged();
}

QString LogBridge::export_dir() const {
    return exporter_.manual_dir_absolute();
}

void LogBridge::on_log(const std::string& module, const std::string& method, LogLevel level,
    const std::string& message) {
    // 日志可能来自任意线程：统一排队到主线程处理
    const QString line = QStringLiteral("[%1] %2::%3  %4")
                             .arg(level_text(level), QString::fromStdString(module),
                                 QString::fromStdString(method), QString::fromStdString(message));
    const int level_value = static_cast<int>(level);
    const QString level_name = level_text(level);
    QMetaObject::invokeMethod(
        this,
        [this, line, level_value, level_name]() {
            QVariantMap item;
            item.insert(QStringLiteral("level"), level_value);
            item.insert(QStringLiteral("levelText"), level_name);
            item.insert(QStringLiteral("text"), line);
            messages_.append(item);
            dirty_ = true;
        },
        Qt::QueuedConnection);
}

void LogBridge::flush_pending() {
    if (!dirty_) {
        return;
    }
    dirty_ = false;
    if (messages_.size() > kMaxMessages) {
        messages_ = messages_.mid(messages_.size() - kMaxMessages);
    }
    emit messagesChanged();
}

void LogBridge::clear() {
    messages_.clear();
    emit messagesChanged();
}

bool LogBridge::exportLogs() {
    QStringList lines;
    for (const auto& item : messages_) {
        lines.append(item.toMap().value(QStringLiteral("text")).toString());
    }
    QString error;
    const bool ok = exporter_.export_log(lines.join(QLatin1Char('\n')), &error);
    if (!ok) {
        emit statusMessage(QStringLiteral("日志导出失败：") + error);
        return false;
    }
    // 导出器内部按时间戳生成文件名，不回传路径：取目录中最新写入的那份作为本次结果
    last_export_path_ = newest_export_file(export_dir());
    emit exportChanged();
    return true;
}

void LogBridge::setLevelFilter(int level) {
    level_filter_ = std::clamp(level, 0, 4);
    DebugLog::instance().set_log_sink_level("qt_ui", static_cast<LogLevel>(level_filter_));
    auto& config = AppConfig::instance();
    config.set_value<int>("app.log.ui_log_level", level_filter_);
    config.save_all();
    emit levelFilterChanged();
}
