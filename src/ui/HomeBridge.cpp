/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "HomeBridge.h"

#include "AppConfig.h"
#include "DebugLog.h"
#include "DeviceController.h"
#include "ModuleManager.h"
#include "RuleManager.h"
#include "WaveLibrary.h"

#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace {

    constexpr qreal kMiniWindowOpacityMin = 0.3; ///< 迷你窗透明度下限（与滑杆量程一致）

    /// @brief 透明度量化到两位小数（滑杆按 0.05 步进，避免 0.599999… 落盘）
    qreal quantize_opacity(qreal opacity) {
        return std::round(std::clamp(opacity, kMiniWindowOpacityMin, 1.0) * 100.0) / 100.0;
    }

} // namespace

HomeBridge::HomeBridge(DeviceController* device, QObject* parent)
    : QObject(parent)
    , device_(device) {
}

void HomeBridge::initialize() {
    if (device_ != nullptr) {
        // 上限不再取 APP 回传值：它属于通道配置（app.channels.<通道>），由 DeviceController 同源提供
        connect(device_, &DeviceController::strengthFeedback, this,
            [this](int a_strength, int b_strength, int, int) {
                const bool changed = a_strength != strength_a_ || b_strength != strength_b_;
                strength_a_ = a_strength;
                strength_b_ = b_strength;
                if (changed) {
                    LOG_MODULE("HomeBridge", "strengthFeedback", LOG_DEBUG,
                        "通道强度回执: A=" << strength_a_ << " B=" << strength_b_);
                }
                emit dataChanged();
            });
        connect(device_, &DeviceController::connectedChanged, this, &HomeBridge::dataChanged);
        // 通道状态包含输出可用性，蓝牙 / 中转任一路变化都要重算
        connect(device_, &DeviceController::outputReadyChanged, this, &HomeBridge::dataChanged);
        // 设备列表与默认设备变化：下拉重排，通道上下限随默认设备切换
        connect(device_, &DeviceController::outputDevicesChanged, this, &HomeBridge::deviceChanged);
        connect(device_, &DeviceController::defaultDeviceChanged, this, &HomeBridge::deviceChanged);
        connect(device_, &DeviceController::limitsChanged, this, &HomeBridge::dataChanged);
        // 波形下发状态（蓝牙帧推进）单独走一条信号，避免每帧连带刷新整张看板
        connect(device_, &DeviceController::waveOutputChanged, this, &HomeBridge::refresh_wave_state);
        refresh_wave_state();
    }

    auto& rules = RuleManager::instance();
    connect(&rules, &RuleManager::rule_result_changed, this, [this](const QString&, const QString&, int) {
        emit dataChanged();
    });
    connect(&rules, &RuleManager::rules_changed, this, &HomeBridge::dataChanged);

    auto& modules = ModuleManager::instance();
    connect(&modules, &ModuleManager::value_changed, this, [this](const QString&, const QString&, int) {
        emit dataChanged();
    });
    connect(&modules, &ModuleManager::period_changed, this, &HomeBridge::dataChanged);
    connect(&modules, &ModuleManager::plugin_state_changed, this, &HomeBridge::dataChanged);

    connect(&WaveLibrary::instance(), &WaveLibrary::current_changed, this, &HomeBridge::dataChanged);

    auto& config = AppConfig::instance();
    mini_window_x_ = config.get_value<int>("app.ui.mini_window.x", 0);
    mini_window_y_ = config.get_value<int>("app.ui.mini_window.y", 0);
    mini_window_width_ = config.get_value<int>("app.ui.mini_window.width", 0);
    mini_window_height_ = config.get_value<int>("app.ui.mini_window.height", 0);
    mini_window_locked_ = config.get_value<bool>("app.ui.mini_window.locked", false);
    mini_window_opacity_ = quantize_opacity(config.get_value<double>("app.ui.mini_window.opacity", 1.0));
    // 大小档位只认三档，未知值（手改配置 / 旧版本残留）回落到 medium
    const QString size = QString::fromStdString(config.get_value<std::string>("app.ui.mini_window.size",
        "medium"));
    mini_window_size_ = (size == QStringLiteral("small") || size == QStringLiteral("large"))
        ? size
        : QStringLiteral("medium");

    LOG_MODULE("HomeBridge", "initialize", LOG_INFO, "首页桥接初始化完成");
}

int HomeBridge::limit_a() const {
    return device_ == nullptr ? 0 : device_->channel_limit(QStringLiteral("A"));
}

int HomeBridge::limit_b() const {
    return device_ == nullptr ? 0 : device_->channel_limit(QStringLiteral("B"));
}

QVariantList HomeBridge::output_devices() const {
    return device_ == nullptr ? QVariantList() : device_->output_devices();
}

QString HomeBridge::default_device() const {
    return device_ == nullptr ? QString() : device_->default_device_address();
}

int HomeBridge::default_device_index() const {
    return device_ == nullptr ? -1 : device_->default_device_index();
}

QString HomeBridge::wave_a() const {
    const QString name = WaveLibrary::instance().current("A");
    return name.isEmpty() ? QStringLiteral("未选择") : name;
}

QString HomeBridge::wave_b() const {
    const QString name = WaveLibrary::instance().current("B");
    return name.isEmpty() ? QStringLiteral("未选择") : name;
}

bool HomeBridge::running_a() const {
    return RuleManager::instance().get_channel_enabled("A");
}

bool HomeBridge::running_b() const {
    return RuleManager::instance().get_channel_enabled("B");
}

QVariantList HomeBridge::modules_a() const {
    return build_modules(QStringLiteral("A"));
}

QVariantList HomeBridge::modules_b() const {
    return build_modules(QStringLiteral("B"));
}

QVariantList HomeBridge::rules_a() const {
    return build_rules(QStringLiteral("A"));
}

QVariantList HomeBridge::rules_b() const {
    return build_rules(QStringLiteral("B"));
}

void HomeBridge::saveMiniWindowState(int x, int y, int width, int height) {
    const bool geometry_changed = x != mini_window_x_ || y != mini_window_y_
        || width != mini_window_width_ || height != mini_window_height_;
    if (!geometry_changed && !mini_window_state_dirty_) {
        return;
    }
    mini_window_x_ = x;
    mini_window_y_ = y;
    mini_window_width_ = width;
    mini_window_height_ = height;
    mini_window_state_dirty_ = false;

    auto& config = AppConfig::instance();
    config.set_value_with_name<int>("app.ui.mini_window.x", mini_window_x_, "user");
    config.set_value_with_name<int>("app.ui.mini_window.y", mini_window_y_, "user");
    config.set_value_with_name<int>("app.ui.mini_window.width", mini_window_width_, "user");
    config.set_value_with_name<int>("app.ui.mini_window.height", mini_window_height_, "user");
    config.set_value_with_name<double>("app.ui.mini_window.opacity", mini_window_opacity_, "user");
    config.set_value_with_name<std::string>("app.ui.mini_window.size", mini_window_size_.toStdString(), "user");
    config.save_all();
    LOG_MODULE("HomeBridge", "saveMiniWindowState", LOG_DEBUG,
        "迷你窗状态已保存: " << mini_window_x_ << "," << mini_window_y_
                             << " " << mini_window_width_ << "x" << mini_window_height_
                             << " 透明度 " << mini_window_opacity_
                             << " 档位 " << mini_window_size_.toStdString());
}

void HomeBridge::setMiniWindowOpacity(qreal opacity) {
    const qreal clamped = quantize_opacity(opacity);
    if (qFuzzyCompare(clamped, mini_window_opacity_)) {
        return;
    }
    mini_window_opacity_ = clamped;
    mini_window_state_dirty_ = true;
    emit miniWindowOpacityChanged();
}

void HomeBridge::setMiniWindowSize(const QString& size) {
    if (size != QStringLiteral("small") && size != QStringLiteral("medium") && size != QStringLiteral("large")) {
        return;
    }
    if (size == mini_window_size_) {
        return;
    }
    mini_window_size_ = size;
    mini_window_state_dirty_ = true;
    emit miniWindowSizeChanged();
}

void HomeBridge::setMiniWindowWave(const QVariantList& wave_a, const QVariantList& wave_b) {
    mini_window_wave_a_ = wave_a;
    mini_window_wave_b_ = wave_b;
    emit miniWindowWaveChanged();
}

void HomeBridge::toggleChannel(const QString& channel) {
    auto& rules = RuleManager::instance();
    const bool enabled = !rules.get_channel_enabled(channel.toStdString());
    rules.set_channel_enabled(channel.toStdString(), enabled);
    LOG_MODULE("HomeBridge", "toggleChannel", LOG_INFO,
        channel.toStdString() + (enabled ? " 通道已启用" : " 通道已关闭"));
    emit dataChanged();
}

int HomeBridge::channel_index(const QString& channel) const {
    return channel == QStringLiteral("A") ? 1 : 2;
}

int HomeBridge::clamp_strength(const QString& channel, int value) const {
    // 钳制值与展示上限同源：界面看到的「上限」就是强度的硬上限
    const int limit = (channel == QStringLiteral("A")) ? limit_a() : limit_b();
    return std::clamp(value, 0, std::min(limit, 200));
}

QString HomeBridge::channel_state(const QString& channel) const {
    const bool running = (channel == QStringLiteral("A")) ? running_a() : running_b();
    if (!running) {
        return QStringLiteral("idle");
    }
    if (device_ != nullptr && device_->output_available()) {
        return QStringLiteral("running");
    }
    return QStringLiteral("runningNoOutput");
}

void HomeBridge::apply_strength(const QString& channel, int value) {
    const int clamped = clamp_strength(channel, value);
    if (channel == QStringLiteral("A")) {
        strength_a_ = clamped;
    }
    else {
        strength_b_ = clamped;
    }
    if (device_ != nullptr && device_->output_available()) {
        device_->sendStrength(channel_index(channel), 2, clamped);
    }
    emit dataChanged();
}

void HomeBridge::setChannelStrength(const QString& channel, int value) {
    apply_strength(channel, value);
}

void HomeBridge::adjustChannelStrength(const QString& channel, int delta) {
    const int current = (channel == QStringLiteral("A")) ? strength_a_ : strength_b_;
    apply_strength(channel, current + delta);
}

void HomeBridge::requestWaveSelect(const QString& channel) {
    emit channelWaveSelectRequested(channel);
}

void HomeBridge::toggleMiniWindowLock() {
    mini_window_locked_ = !mini_window_locked_;
    auto& config = AppConfig::instance();
    config.set_value_with_name<bool>("app.ui.mini_window.locked", mini_window_locked_, "user");
    config.save_all();
    emit miniWindowLockedChanged();
    LOG_MODULE("HomeBridge", "toggleMiniWindowLock", LOG_INFO,
        mini_window_locked_ ? "迷你窗已锁定" : "迷你窗已解锁");
}

void HomeBridge::setDefaultDeviceIndex(int index) {
    if (device_ == nullptr) {
        return;
    }
    device_->setDefaultDeviceByIndex(index);
    emit deviceChanged();
}

QVariantList HomeBridge::build_modules(const QString& channel) const {
    QVariantList list;
    auto& modules = ModuleManager::instance();
    for (const auto& name : modules.get_modules_for_channel(channel.toStdString())) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), QString::fromStdString(name));
        item.insert(QStringLiteral("periodMs"), modules.get_module_min_period_ms(name));
        list.append(item);
    }
    return list;
}

void HomeBridge::refresh_wave_state() {
    const bool sending_a = device_ != nullptr && device_->wave_sending(QStringLiteral("A"));
    const int frame_a = sending_a ? device_->wave_frame(QStringLiteral("A")) : 0;
    const int frames_a = sending_a ? device_->wave_frames(QStringLiteral("A")) : 0;
    const bool sending_b = device_ != nullptr && device_->wave_sending(QStringLiteral("B"));
    const int frame_b = sending_b ? device_->wave_frame(QStringLiteral("B")) : 0;
    const int frames_b = sending_b ? device_->wave_frames(QStringLiteral("B")) : 0;
    if (sending_a == wave_sending_a_ && frame_a == wave_frame_a_ && frames_a == wave_frames_a_
        && sending_b == wave_sending_b_ && frame_b == wave_frame_b_ && frames_b == wave_frames_b_) {
        return;
    }
    wave_sending_a_ = sending_a;
    wave_frame_a_ = frame_a;
    wave_frames_a_ = frames_a;
    wave_sending_b_ = sending_b;
    wave_frame_b_ = frame_b;
    wave_frames_b_ = frames_b;
    emit waveStateChanged();
}

QVariantList HomeBridge::build_rules(const QString& channel) const {
    QVariantList list;
    auto& rules = RuleManager::instance();
    for (const auto& name : rules.get_rule_names()) {
        bool has_channel = false;
        for (const auto& parent : rules.get_rule_parents(name)) {
            if (parent.type == ParentType::CHANNEL && parent.channel == channel.toStdString()) {
                has_channel = true;
                break;
            }
        }
        if (!has_channel) {
            continue;
        }
        QVariantMap item;
        item.insert(QStringLiteral("name"), QString::fromStdString(name));
        const auto last = rules.get_rule_last_result(name);
        item.insert(QStringLiteral("value"), last.has_value() ? QVariant(last.value()) : QVariant());
        list.append(item);
    }
    return list;
}
