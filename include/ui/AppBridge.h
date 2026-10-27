/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "AppVersion.h"

// AppBridge - 应用级状态桥接（QML 上下文属性 app）
// 职责：向 QML 暴露只读应用信息（名称/版本）、页面导航状态与一次性反馈浮层（Toast）。
//      界面只做属性绑定，交互由 UiConnector 在 C++ 侧连接。
// 反馈面分工：一次性消息走 Toast（toastText / toastLevel，3 秒自动消失）；
//      状态栏只显示 QML 侧组合的常驻摘要，不再承载一次性文案。
class AppBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appName READ app_name NOTIFY appInfoChanged)
    Q_PROPERTY(QString appVersion READ app_version NOTIFY appInfoChanged)
    Q_PROPERTY(int currentPage READ current_page WRITE set_current_page NOTIFY currentPageChanged)
    Q_PROPERTY(int pageCount READ page_count CONSTANT)
    // 兼容保留：setStatus 已转调 showToast，状态栏不再绑定这两个属性
    Q_PROPERTY(QString statusText READ status_text NOTIFY statusTextChanged)
    Q_PROPERTY(int statusLevel READ status_level NOTIFY statusTextChanged)
    Q_PROPERTY(QString toastText READ toast_text NOTIFY toastChanged)
    Q_PROPERTY(int toastLevel READ toast_level NOTIFY toastChanged)

public:
    enum Page {
        HomePage = 0,
        ConfigPage = 1,
        ModulePage = 2,
        AboutPage = 3,
        PluginCenterPage = 4,
        WavePage = 5,
        MiniGamePage = 6,
        LogPage = 7
    };
    Q_ENUM(Page)

    /// @brief 反馈消息级别（0=info / 1=success / 2=warning / 3=danger），同时决定 Toast 描边色
    enum StatusLevel {
        StatusInfo = 0,
        StatusSuccess = 1,
        StatusWarning = 2,
        StatusDanger = 3
    };
    Q_ENUM(StatusLevel)

    explicit AppBridge(QObject* parent = nullptr);

    /// @brief 从配置读取应用名称与版本
    void initialize();

    QString app_name() const { return app_name_; }
    QString app_version() const { return app_version_; }
    int current_page() const { return current_page_; }
    int page_count() const { return static_cast<int>(Page::LogPage) + 1; }
    QString status_text() const { return status_text_; }
    int status_level() const { return status_level_; }
    QString toast_text() const { return toast_text_; }
    int toast_level() const { return toast_level_; }

    void set_current_page(int page);
    Q_INVOKABLE void navigate(int page);

    /// @brief 打开指定插件的详情页（目标插件不存在时只切页并提示）
    /// @param plugin_key 插件键（插件文件名或模块标识，两者都接受）
    Q_INVOKABLE void openPluginDetail(const QString& plugin_key);

    /// @brief 弹出一次性反馈浮层；空文本立即隐藏
    Q_INVOKABLE void showToast(const QString& text, int level = StatusInfo);

    /// @brief 兼容旧调用点：等价于 showToast
    Q_INVOKABLE void setStatus(const QString& text, int level = StatusInfo);

signals:
    void appInfoChanged();
    void currentPageChanged();
    void statusTextChanged();
    void toastChanged();

    /// @brief 请求定位到指定插件详情（由 UiConnector 在插件中心页设置选中项）
    /// @param plugin_key 插件键
    void pluginDetailRequested(const QString& plugin_key);

private:
    QString app_name_ = QStringLiteral("DG-LAB-Client");
    QString app_version_ = QStringLiteral(APP_VERSION);
    int current_page_ = HomePage;
    QString status_text_;
    int status_level_ = StatusInfo;
    QString toast_text_;
    int toast_level_ = StatusInfo;
    QTimer toast_timer_;
};
