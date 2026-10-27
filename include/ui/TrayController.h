/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QSystemTrayIcon>

class QAction;
class QMenu;
class QQuickWindow;

// TrayController - 系统托盘常驻（QML 上下文属性 tray）
// 职责：托盘图标与三项菜单（显示主窗口 / 显示迷你窗 / 退出）、双击图标恢复主窗口，
//      以及主窗口关闭时的落点：托盘可用则隐藏窗口，不可用则退出应用。
class TrayController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit TrayController(QObject* parent = nullptr);
    ~TrayController() override;

    /// @brief 创建托盘图标与菜单；系统托盘不可用时只记录一次告警并降级
    void initialize();

    /// @brief 绑定主窗口与迷你窗（engine.load 之后调用）
    void attach(QQuickWindow* main_window, QQuickWindow* mini_window);

    bool available() const { return available_; }
    /// @brief 托盘菜单项文本（启动日志与自检用）
    QStringList menu_texts() const;

    /// @brief 主窗口关闭请求：托盘可用时隐藏主窗口，否则结束应用
    Q_INVOKABLE void handleMainWindowClose();

signals:
    /// @brief 托盘发起退出（发出时配置已保存）
    void quitRequested();

private slots:
    void on_show_main_window();
    void on_show_mini_window();
    void on_quit();
    void on_tray_activated(QSystemTrayIcon::ActivationReason reason);

private:
    /// @brief 恢复窗口：显示、抬升并请求激活
    void show_window(QQuickWindow* window);
    /// @brief 保存配置、撤掉托盘图标并结束事件循环
    void quit_application();

    QSystemTrayIcon* tray_ = nullptr;
    QMenu* menu_ = nullptr;
    QAction* show_main_action_ = nullptr;
    QAction* show_mini_action_ = nullptr;
    QAction* quit_action_ = nullptr;
    QPointer<QQuickWindow> main_window_;
    QPointer<QQuickWindow> mini_window_;
    bool available_ = false;
};
