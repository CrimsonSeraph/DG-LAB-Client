/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

class AppBridge;
class DeviceController;
class HomeBridge;
class ThemeManager;
class QQuickItem;

// ============================================
// UiConnector - QML 与 C++ 的连接集中点
// ============================================
// 约定：QML 不写任何信号处理器（onClicked/Connections），每个交互控件都有稳定的
//      objectName；加载完成后由本类按 objectName 显式建立连接。
//      列表/中继器动态生成的委托（如主题预设项）由 watch_list() 沿可视子树扫描热区连接。
class UiConnector : public QObject {
    Q_OBJECT

public:
    UiConnector(AppBridge* app, HomeBridge* home, ThemeManager* theme, DeviceController* device,
        QObject* parent = nullptr);

    /// @brief 在 engine.load 之后调用，连接根对象下的全部交互控件
    void attach(QObject* root_object);

private slots:
    // 导航
    void on_nav_button_clicked();
    void on_home_module_entry_clicked();
    void on_rule_editor_clicked();
    // 首页通道
    void on_channel_toggle_clicked();
    void on_strength_modified();
    void on_strength_adjust_clicked();
    void on_wave_select_clicked();
    // 配置：连接
    void on_config_connect_clicked();
    // 配置：主题
    void on_theme_select_clicked();
    void on_theme_custom_clicked();
    void on_theme_preset_clicked();
    void on_custom_theme_save_clicked();
    void on_custom_primary_clicked();
    void on_custom_secondary_clicked();
    void on_primary_color_accepted();
    void on_secondary_color_accepted();
    void on_tracked_children_changed();

private:
    void connect_clicked(QObject* root_object, const char* object_name, const char* slot);
    void open_dialog(const char* object_name);
    void close_dialog(const char* object_name);
    void watch_list(QObject* root_object, const char* list_name, const QString& item_name, const char* slot);
    void scan_dynamic_items(QQuickItem* parent, const QString& item_name, const char* slot);
    QObject* find(const char* object_name) const;

    struct ListWatch {
        QQuickItem* content = nullptr;
        QString item_name;
        const char* slot = nullptr;
    };

    AppBridge* app_ = nullptr;
    HomeBridge* home_ = nullptr;
    ThemeManager* theme_ = nullptr;
    DeviceController* device_ = nullptr;
    QPointer<QObject> root_;
    QList<ListWatch> watches_;
    QSet<QObject*> connected_items_;
};
