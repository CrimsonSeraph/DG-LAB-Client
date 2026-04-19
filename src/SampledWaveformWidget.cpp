/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DebugLog.h"
#include "SampledWaveformWidget.h"

#include <QMutex>
#include <QPainter>
#include <QPen>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <vector>
#include <map>
#include <string>

// ============================================
// 构造/析构（public）
// ============================================

SampledWaveformWidget::SampledWaveformWidget(QWidget* parent)
    : QWidget(parent)
    , sample_interval_ms_(kDefaultSampleIntervalMs)
    , max_amplitude_(kDefaultMaxAmplitude)
    , max_listeners_(kDefaultMaxListeners) {
    ensure_default_listener();

    sample_timer_.setInterval(sample_interval_ms_);
    connect(&sample_timer_, &QTimer::timeout, this, &SampledWaveformWidget::on_sample_timer);
    sample_timer_.start();

    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    LOG_MODULE("SampledWaveformWidget", "SampledWaveformWidget", LOG_DEBUG,
        "初始化完成，采样间隔=" << sample_interval_ms_ << "ms，最大振幅比例=" << max_amplitude_
        << "，最大监听器数量=" << max_listeners_);
}

// ============================================
// 监听器管理（public）
// ============================================

bool SampledWaveformWidget::add_listener(const std::string& name, const QColor& color, double min, double max) {
    if (min >= max) {
        LOG_MODULE("SampledWaveformWidget", "add_listener", LOG_WARN,
            "无效范围: min=" << min << " >= max=" << max << "，使用默认范围 [0,1]");
        min = 0.0;
        max = 1.0;
    }
    QMutexLocker locker(&mutex_);
    if (listeners_.find(name) != listeners_.end()) {
        LOG_MODULE("SampledWaveformWidget", "add_listener", LOG_WARN,
            "监听器已存在: " << name);
        return false;
    }
    if (listeners_.size() >= static_cast<size_t>(max_listeners_)) {
        LOG_MODULE("SampledWaveformWidget", "add_listener", LOG_WARN,
            "监听器数量已达上限 (" << max_listeners_ << ")，无法添加: " << name);
        return false;
    }
    listeners_.emplace(name, ListenerData(color, min, max));
    LOG_MODULE("SampledWaveformWidget", "add_listener", LOG_INFO,
        "添加监听器成功: " << name << ", 颜色: " << color.name().toStdString()
        << ", 范围: [" << min << ", " << max << "], 当前数量: " << listeners_.size());
    update();
    return true;
}

bool SampledWaveformWidget::add_listener(const std::string& name, const QColor& color) {
    return add_listener(name, color, 0.0, 1.0);
}

bool SampledWaveformWidget::remove_listener(const std::string& name) {
    QMutexLocker locker(&mutex_);
    if (name == "default") {
        LOG_MODULE("SampledWaveformWidget", "remove_listener", LOG_WARN,
            "不允许删除默认监听器 \"default\"");
        return false;
    }
    auto it = listeners_.find(name);
    if (it == listeners_.end()) {
        LOG_MODULE("SampledWaveformWidget", "remove_listener", LOG_WARN,
            "监听器不存在: " << name);
        return false;
    }
    listeners_.erase(it);
    LOG_MODULE("SampledWaveformWidget", "remove_listener", LOG_INFO,
        "移除监听器成功: " << name << ", 剩余数量: " << listeners_.size());
    update();
    return true;
}

bool SampledWaveformWidget::set_listener_color(const std::string& name, const QColor& color) {
    QMutexLocker locker(&mutex_);
    auto it = listeners_.find(name);
    if (it == listeners_.end()) {
        LOG_MODULE("SampledWaveformWidget", "set_listener_color", LOG_WARN,
            "监听器不存在: " << name);
        return false;
    }
    it->second.color = color;
    LOG_MODULE("SampledWaveformWidget", "set_listener_color", LOG_DEBUG,
        "修改监听器颜色: " << name << " -> " << color.name().toStdString());
    update();
    return true;
}

std::vector<std::string> SampledWaveformWidget::get_listener_names() const {
    QMutexLocker locker(&mutex_);
    std::vector<std::string> names;
    names.reserve(listeners_.size());
    for (const auto& pair : listeners_) {
        names.push_back(pair.first);
    }
    return names;
}

bool SampledWaveformWidget::has_listener(const std::string& name) const {
    QMutexLocker locker(&mutex_);
    return listeners_.find(name) != listeners_.end();
}

void SampledWaveformWidget::set_max_listeners(int limit) {
    if (limit < 1) {
        LOG_MODULE("SampledWaveformWidget", "set_max_listeners", LOG_WARN,
            "无效上限值 " << limit << "，保持原值 " << max_listeners_);
        return;
    }
    max_listeners_ = limit;
    LOG_MODULE("SampledWaveformWidget", "set_max_listeners", LOG_DEBUG,
        "最大监听器数量更新为: " << max_listeners_);
}

// ============================================
// 数据输入（public）
// ============================================

void SampledWaveformWidget::input_data(const std::string& listener_name, double value) {
    QMutexLocker locker(&mutex_);
    auto it = listeners_.find(listener_name);
    if (it == listeners_.end()) {
        if (listener_name == "default") {
            ensure_default_listener();
            it = listeners_.find("default");
            if (it == listeners_.end()) {
                LOG_MODULE("SampledWaveformWidget", "input_data", LOG_ERROR,
                    "无法创建默认监听器");
                return;
            }
        }
        else {
            LOG_MODULE("SampledWaveformWidget", "input_data", LOG_WARN,
                "监听器不存在，数据被丢弃: " << listener_name);
            return;
        }
    }

    ListenerData& data = it->second;
    double min = data.input_min;
    double max = data.input_max;

    // 钳位原始值到 [min, max]
    double clamped = value;
    if (clamped < min) {
        clamped = min;
        LOG_MODULE("SampledWaveformWidget", "input_data", LOG_DEBUG,
            "原始值 " << value << " 低于下限 " << min << "，已钳位");
    }
    else if (clamped > max) {
        clamped = max;
        LOG_MODULE("SampledWaveformWidget", "input_data", LOG_DEBUG,
            "原始值 " << value << " 高于上限 " << max << "，已钳位");
    }

    // 线性映射到 [0, 1]
    double normalized = (clamped - min) / (max - min);
    // 防止浮点误差导致超出 [0,1]
    normalized = qBound(0.0, normalized, 1.0);

    data.latest_value = normalized;
    data.has_new_input = true;

    LOG_MODULE("SampledWaveformWidget", "input_data", LOG_DEBUG,
        "监听器 " << listener_name << " 原始值=" << value
        << " -> 归一化=" << normalized);
}

void SampledWaveformWidget::set_input_range(const std::string& name, double min, double max) {
    if (min >= max) {
        LOG_MODULE("SampledWaveformWidget", "set_input_range", LOG_WARN,
            "无效范围: min=" << min << " >= max=" << max << "，设置被忽略");
        return;
    }
    QMutexLocker locker(&mutex_);
    auto it = listeners_.find(name);
    if (it == listeners_.end()) {
        LOG_MODULE("SampledWaveformWidget", "set_input_range", LOG_WARN,
            "监听器不存在: " << name);
        return;
    }
    it->second.input_min = min;
    it->second.input_max = max;
    LOG_MODULE("SampledWaveformWidget", "set_input_range", LOG_DEBUG,
        "监听器 " << name << " 范围已更新为 [" << min << ", " << max << "]");
}

void SampledWaveformWidget::set_input_range(const std::string& name, int min, int max) {
    set_input_range(name, static_cast<double>(min), static_cast<double>(max));
}

// ============================================
// 全局配置（public）
// ============================================

void SampledWaveformWidget::set_sample_interval_ms(int ms) {
    if (ms <= 0) {
        LOG_MODULE("SampledWaveformWidget", "set_sample_interval_ms", LOG_WARN,
            "无效采样间隔=" << ms << "，保持原值=" << sample_interval_ms_);
        return;
    }
    sample_interval_ms_ = ms;
    sample_timer_.setInterval(ms);
    LOG_MODULE("SampledWaveformWidget", "set_sample_interval_ms", LOG_DEBUG,
        "采样间隔已更新为 " << ms << "ms");
}

void SampledWaveformWidget::set_max_amplitude(double ratio) {
    max_amplitude_ = qBound(0.1, ratio, 1.0);
    LOG_MODULE("SampledWaveformWidget", "set_max_amplitude", LOG_DEBUG,
        "最大振幅比例已更新为 " << max_amplitude_);
    update();
}

// ============================================
// Qt 事件处理（protected）
// ============================================

void SampledWaveformWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const int baseline_y = h - 5;   // 底部边距5像素
    const int top_margin = 5;
    const int left_margin = 0;
    const int right_margin = 0;

    // 绘制灰色虚线基线
    painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
    painter.drawLine(left_margin, baseline_y, w - right_margin, baseline_y);

    // 复制监听器数据快照，避免绘制过程中被修改
    std::map<std::string, ListenerData> snapshot;
    {
        QMutexLocker locker(&mutex_);
        snapshot = listeners_;
    }

    double step = static_cast<double>(w - left_margin - right_margin) / (kMaxSamplePoints - 1);
    double amplitude_range = baseline_y - top_margin;

    for (const auto& pair : snapshot) {
        const ListenerData& data = pair.second;
        // 构建有序采样点（从旧到新）
        QVector<double> ordered_samples;
        ordered_samples.reserve(kMaxSamplePoints);
        for (int i = 0; i < kMaxSamplePoints; ++i) {
            int idx = (data.sample_index + i) % kMaxSamplePoints;
            ordered_samples.append(data.samples[idx]);
        }

        // 构建折线点集
        QVector<QPointF> points;
        points.reserve(kMaxSamplePoints);
        for (int i = 0; i < kMaxSamplePoints; ++i) {
            double x = left_margin + i * step;
            double y = baseline_y - amplitude_range * max_amplitude_ * ordered_samples[i];
            y = qBound(static_cast<double>(top_margin), y, static_cast<double>(baseline_y));
            points.append(QPointF(x, y));
        }

        painter.setPen(QPen(data.color, 2));
        for (int i = 0; i < points.size() - 1; ++i) {
            painter.drawLine(points[i], points[i + 1]);
        }
    }
}

void SampledWaveformWidget::resizeEvent(QResizeEvent* event) {
    update();
}

// ============================================
// 定时器槽函数（private slots）
// ============================================

void SampledWaveformWidget::on_sample_timer() {
    QMutexLocker locker(&mutex_);
    for (auto& pair : listeners_) {
        const std::string& name = pair.first;
        ListenerData& data = pair.second;
        double value_to_sample = data.has_new_input ? data.latest_value : 0.0;
        data.has_new_input = false;
        add_sample(name, value_to_sample);
    }
    update();
}

// ============================================
// 私有辅助函数（private）
// ============================================

void SampledWaveformWidget::add_sample(const std::string& name, double value) {
    auto it = listeners_.find(name);
    if (it == listeners_.end()) {
        LOG_MODULE("SampledWaveformWidget", "add_sample", LOG_ERROR,
            "试图为不存在的监听器添加采样点: " << name);
        return;
    }
    ListenerData& data = it->second;
    data.samples[data.sample_index] = value;
    data.sample_index = (data.sample_index + 1) % kMaxSamplePoints;
}

void SampledWaveformWidget::ensure_default_listener() {
    if (listeners_.find("default") == listeners_.end()) {
        listeners_.emplace("default", ListenerData(Qt::green, 0.0, 1.0));
        LOG_MODULE("SampledWaveformWidget", "ensure_default_listener", LOG_DEBUG,
            "自动创建默认监听器 \"default\"，颜色绿色，范围 [0,1]");
    }
}
