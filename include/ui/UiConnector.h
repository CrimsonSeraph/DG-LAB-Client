/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>

class AppBridge;
class HomeBridge;

// ============================================
// UiConnector - QML 与 C++ 的连接集中点
// ============================================
// 约定：QML 不写任何信号处理器（onClicked/Connections），每个交互控件都有稳定的
//      objectName；加载完成后由本类按 objectName 显式建立连接。
//      后续每新增交互控件，必须同步在本类中登记。
class UiConnector : public QObject {
    Q_OBJECT

public:
    UiConnector(AppBridge* app, HomeBridge* home, QObject* parent = nullptr);

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

private:
    void connect_navigation(QObject* root_object);
    void connect_home(QObject* root_object);
    /// @brief 按 objectName 连接点击信号到指定槽
    void connect_clicked(QObject* root_object, const char* object_name, const char* slot);

    AppBridge* app_ = nullptr;
    HomeBridge* home_ = nullptr;
};
