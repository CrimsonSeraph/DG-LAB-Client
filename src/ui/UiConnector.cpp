/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "UiConnector.h"

#include "AppBridge.h"
#include "DebugLog.h"

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

} // namespace

UiConnector::UiConnector(AppBridge* bridge, QObject* parent)
    : QObject(parent)
    , bridge_(bridge) {
}

void UiConnector::attach(QObject* root_object) {
    if (root_object == nullptr) {
        LOG_MODULE("UiConnector", "attach", LOG_ERROR, "根对象为空，无法建立 QML 连接");
        return;
    }
    connect_navigation(root_object);
    LOG_MODULE("UiConnector", "attach", LOG_INFO, "QML 连接已完成");
}

void UiConnector::on_nav_button_clicked() {
    auto* button = sender();
    if (button == nullptr) {
        return;
    }
    const auto page = nav_bindings().value(button->objectName(), -1);
    if (page < 0) {
        LOG_MODULE("UiConnector", "on_nav_button_clicked", LOG_WARN,
            "未登记的导航按钮: " + button->objectName().toStdString());
        return;
    }
    bridge_->navigate(page);
}

void UiConnector::connect_navigation(QObject* root_object) {
    for (auto it = nav_bindings().constBegin(); it != nav_bindings().constEnd(); ++it) {
        auto* button = root_object->findChild<QObject*>(it.key());
        if (button == nullptr) {
            LOG_MODULE("UiConnector", "connect_navigation", LOG_WARN,
                "未找到导航按钮: " + it.key().toStdString());
            continue;
        }
        // 使用字符串形式的信号/槽：避免依赖 QtQuickTemplates2 的公开头文件
        QObject::connect(button, SIGNAL(clicked()), this, SLOT(on_nav_button_clicked()));
    }
}
