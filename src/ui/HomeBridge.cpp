/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "HomeBridge.h"

#include "DebugLog.h"
#include "DeviceController.h"
#include "ModuleManager.h"
#include "RuleManager.h"
#include "WaveLibrary.h"

#include <QVariantMap>

#include <algorithm>

HomeBridge::HomeBridge(DeviceController* device, QObject* parent)
    : QObject(parent)
    , device_(device) {
}

void HomeBridge::initialize() {
    if (device_ != nullptr) {
        // 上限不再取 APP 回传值：它属于通道配置（app.channels.<通道>），由 DeviceController 同源提供
        connect(device_, &DeviceController::strengthFeedback, this,
            [this](int a_strength, int b_strength, int, int) {
                strength_a_ = a_strength;
                strength_b_ = b_strength;
                emit dataChanged();
            });
        connect(device_, &DeviceController::connectedChanged, this, &HomeBridge::dataChanged);
        // 通道状态包含输出可用性，蓝牙 / 中转任一路变化都要重算
        connect(device_, &DeviceController::outputReadyChanged, this, &HomeBridge::dataChanged);
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

    LOG_MODULE("HomeBridge", "initialize", LOG_INFO, "首页桥接初始化完成");
}

int HomeBridge::limit_a() const {
    return device_ == nullptr ? 0 : device_->channel_limit(QStringLiteral("A"));
}

int HomeBridge::limit_b() const {
    return device_ == nullptr ? 0 : device_->channel_limit(QStringLiteral("B"));
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
