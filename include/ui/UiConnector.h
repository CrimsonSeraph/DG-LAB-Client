/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>

class AppBridge;

// ============================================
// UiConnector - QML 与 C++ 的连接集中点
// ============================================
// 约定：QML 不写任何信号处理器（onClicked/Connections），每个交互控件都有稳定的
//      objectName；加载完成后由本类按 objectName 显式建立连接。
//      后续每新增交互控件，必须同步在 connect_navigation() 等方法中登记。
class UiConnector : public QObject {
    Q_OBJECT

public:
    explicit UiConnector(AppBridge* bridge, QObject* parent = nullptr);

    /// @brief 在 engine.load 之后调用，连接根对象下的全部交互控件
    void attach(QObject* root_object);

private slots:
    /// @brief 导航按钮点击：按发送者 objectName 切换到对应页面
    void on_nav_button_clicked();

private:
    void connect_navigation(QObject* root_object);

    AppBridge* bridge_ = nullptr;
};
