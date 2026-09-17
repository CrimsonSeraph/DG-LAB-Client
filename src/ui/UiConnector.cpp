/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "UiConnector.h"

#include "AppBridge.h"
#include "DebugLog.h"
#include "HomeBridge.h"

#include <QHash>
#include <QObject>

namespace {

    /// @brief 导航按钮 objectName -> 页面索引
    const QHash<QString, int>& nav_bindings() {
        static const QHash<QString, int> bindings = {
            { QStringLiteral("navHomeButton"), AppBridge::HomePage },
            { QStringLiteral("navConfigButton"), AppBridge::ConfigPage },
            { QStringLiteral("navModuleButton"), AppBridge::ModulePage },
            { QStringLiteral("navAboutButton"), AppBridge::AboutPage }
        };
        return bindings;
    }

    /// @brief 从 "channelAStrengthSpin" 这类名字中解析通道（A/B）
    QString channel_of(const QString& object_name) {
        const int index = object_name.indexOf(QStringLiteral("channel"));
        if (index < 0 || index + 8 >= object_name.size()) {
            return QString();
        }
        return object_name.mid(index + 7, 1);
    }

} // namespace

UiConnector::UiConnector(AppBridge* app, HomeBridge* home, QObject* parent)
    : QObject(parent)
    , app_(app)
    , home_(home) {
}

void UiConnector::attach(QObject* root_object) {
    if (root_object == nullptr) {
        LOG_MODULE("UiConnector", "attach", LOG_ERROR, "根对象为空，无法建立 QML 连接");
        return;
    }
    connect_navigation(root_object);
    connect_home(root_object);
    LOG_MODULE("UiConnector", "attach", LOG_INFO, "QML 连接已完成");
}

void UiConnector::connect_clicked(QObject* root_object, const char* object_name, const char* slot) {
    auto* target = root_object->findChild<QObject*>(QString::fromLatin1(object_name));
    if (target == nullptr) {
        LOG_MODULE("UiConnector", "connect_clicked", LOG_WARN,
            std::string("未找到控件: ") + object_name);
        return;
    }
    QObject::connect(target, SIGNAL(clicked()), this, slot);
}

void UiConnector::connect_navigation(QObject* root_object) {
    for (auto it = nav_bindings().constBegin(); it != nav_bindings().constEnd(); ++it) {
        auto* button = root_object->findChild<QObject*>(it.key());
        if (button == nullptr) {
            LOG_MODULE("UiConnector", "connect_navigation", LOG_WARN,
                "未找到导航按钮: " + it.key().toStdString());
            continue;
        }
        QObject::connect(button, SIGNAL(clicked()), this, SLOT(on_nav_button_clicked()));
    }
    connect_clicked(root_object, "homeModuleEntryButton", SLOT(on_home_module_entry_clicked()));
    connect_clicked(root_object, "homeRuleEditorButton", SLOT(on_rule_editor_clicked()));
    connect_clicked(root_object, "configRuleEditorButton", SLOT(on_rule_editor_clicked()));
}

void UiConnector::connect_home(QObject* root_object) {
    connect_clicked(root_object, "channelAStartButton", SLOT(on_channel_toggle_clicked()));
    connect_clicked(root_object, "channelBStartButton", SLOT(on_channel_toggle_clicked()));
    connect_clicked(root_object, "channelAStrengthIncrease", SLOT(on_strength_adjust_clicked()));
    connect_clicked(root_object, "channelAStrengthDecrease", SLOT(on_strength_adjust_clicked()));
    connect_clicked(root_object, "channelBStrengthIncrease", SLOT(on_strength_adjust_clicked()));
    connect_clicked(root_object, "channelBStrengthDecrease", SLOT(on_strength_adjust_clicked()));
    connect_clicked(root_object, "channelASelectWaveButton", SLOT(on_wave_select_clicked()));
    connect_clicked(root_object, "channelBSelectWaveButton", SLOT(on_wave_select_clicked()));

    const char* spins[] = { "channelAStrengthSpin", "channelBStrengthSpin" };
    for (const char* name : spins) {
        auto* spin = root_object->findChild<QObject*>(QString::fromLatin1(name));
        if (spin == nullptr) {
            continue;
        }
        QObject::connect(spin, SIGNAL(valueModified()), this, SLOT(on_strength_modified()));
    }
}

void UiConnector::on_nav_button_clicked() {
    auto* button = sender();
    if (button == nullptr) {
        return;
    }
    const auto page = nav_bindings().value(button->objectName(), -1);
    if (page >= 0) {
        app_->navigate(page);
    }
}

void UiConnector::on_home_module_entry_clicked() {
    app_->navigate(AppBridge::ModulePage);
}

void UiConnector::on_rule_editor_clicked() {
    // 规则可视化编辑器在阶段 9 接入
    app_->setStatus(QStringLiteral("规则编辑器将在后续阶段实现"));
}

void UiConnector::on_channel_toggle_clicked() {
    auto* button = sender();
    if (button == nullptr || home_ == nullptr) {
        return;
    }
    home_->toggleChannel(channel_of(button->objectName()));
}

void UiConnector::on_strength_modified() {
    auto* spin = sender();
    if (spin == nullptr || home_ == nullptr) {
        return;
    }
    const QString channel = channel_of(spin->objectName());
    home_->setChannelStrength(channel, spin->property("value").toInt());
}

void UiConnector::on_strength_adjust_clicked() {
    auto* button = sender();
    if (button == nullptr || home_ == nullptr) {
        return;
    }
    const QString name = button->objectName();
    const QString channel = channel_of(name);
    const int delta = name.contains(QStringLiteral("Increase")) ? 1 : -1;
    home_->adjustChannelStrength(channel, delta);
}

void UiConnector::on_wave_select_clicked() {
    auto* button = sender();
    if (button == nullptr || home_ == nullptr) {
        return;
    }
    home_->requestWaveSelect(channel_of(button->objectName()));
    app_->setStatus(QStringLiteral("波形库将在阶段 6 实现"));
}
