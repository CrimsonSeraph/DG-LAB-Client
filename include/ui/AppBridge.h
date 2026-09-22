/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QString>

// ============================================
// AppBridge - 应用级状态桥接（QML 上下文属性 app）
// ============================================
// 职责：向 QML 暴露只读应用信息（名称/版本）与页面导航状态，
//      并提供底部状态栏文本。界面只做属性绑定，交互由 UiConnector 在 C++ 侧连接。
class AppBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appName READ app_name NOTIFY appInfoChanged)
    Q_PROPERTY(QString appVersion READ app_version NOTIFY appInfoChanged)
    Q_PROPERTY(int currentPage READ current_page WRITE set_current_page NOTIFY currentPageChanged)
    Q_PROPERTY(int pageCount READ page_count CONSTANT)
    Q_PROPERTY(QString statusText READ status_text NOTIFY statusTextChanged)

public:
    enum Page {
        HomePage = 0,
        ConfigPage = 1,
        ModulePage = 2,
        AboutPage = 3
    };
    Q_ENUM(Page)

    explicit AppBridge(QObject* parent = nullptr);

    /// @brief 从配置读取应用名称与版本
    void initialize();

    QString app_name() const { return app_name_; }
    QString app_version() const { return app_version_; }
    int current_page() const { return current_page_; }
    int page_count() const { return 4; }
    QString status_text() const { return status_text_; }

    void set_current_page(int page);
    Q_INVOKABLE void navigate(int page);

    /// @brief 设置底部状态栏文本
    Q_INVOKABLE void setStatus(const QString& text);

signals:
    void appInfoChanged();
    void currentPageChanged();
    void statusTextChanged();

private:
    QString app_name_ = QStringLiteral("DG-LAB-Client");
    QString app_version_ = QStringLiteral("2.0.0");
    int current_page_ = HomePage;
    QString status_text_;
};
