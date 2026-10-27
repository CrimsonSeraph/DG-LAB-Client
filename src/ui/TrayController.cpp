/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "TrayController.h"

#include "AppConfig.h"
#include "DebugLog.h"

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QQuickWindow>

namespace {

    /// @brief 托盘图标：复用窗口图标资源，按托盘像素尺寸取多档
    QIcon tray_icon() {
        QIcon icon;
        icon.addFile(QStringLiteral(":/icon/assets/icon/dglab-client-256.png"));
        icon.addFile(QStringLiteral(":/icon/assets/icon/dglab-client-64.png"));
        icon.addFile(QStringLiteral(":/icon/assets/icon/dglab-client-32.png"));
        icon.addFile(QStringLiteral(":/icon/assets/icon/dglab-client-16.png"));
        return icon;
    }

} // namespace

TrayController::TrayController(QObject* parent)
    : QObject(parent) {
}

TrayController::~TrayController() {
    if (tray_ != nullptr) {
        tray_->setContextMenu(nullptr);
        tray_->hide();
    }
    delete menu_;
    menu_ = nullptr;
}

void TrayController::initialize() {
    available_ = QSystemTrayIcon::isSystemTrayAvailable();
    if (!available_) {
        LOG_MODULE("TrayController", "initialize", LOG_WARN,
            "系统托盘不可用：主窗口关闭将直接退出，不提供托盘常驻");
        return;
    }

    tray_ = new QSystemTrayIcon(tray_icon(), this);
    tray_->setToolTip(QStringLiteral("DG-LAB-Client"));

    menu_ = new QMenu();
    show_main_action_ = menu_->addAction(QStringLiteral("显示主窗口"));
    show_mini_action_ = menu_->addAction(QStringLiteral("显示迷你窗"));
    quit_action_ = menu_->addAction(QStringLiteral("退出"));
    connect(show_main_action_, &QAction::triggered, this, &TrayController::on_show_main_window);
    connect(show_mini_action_, &QAction::triggered, this, &TrayController::on_show_mini_window);
    connect(quit_action_, &QAction::triggered, this, &TrayController::on_quit);

    tray_->setContextMenu(menu_);
    connect(tray_, &QSystemTrayIcon::activated, this, &TrayController::on_tray_activated);
    tray_->show();

    LOG_MODULE("TrayController", "initialize", LOG_INFO,
        "托盘已就绪，菜单 " << menu_texts().size() << " 项: "
                            << menu_texts().join(QStringLiteral(" / ")).toStdString());
}

void TrayController::attach(QQuickWindow* main_window, QQuickWindow* mini_window) {
    main_window_ = main_window;
    mini_window_ = mini_window;
    if (main_window_.isNull()) {
        LOG_MODULE("TrayController", "attach", LOG_WARN, "主窗口为空，托盘菜单无法恢复主窗口");
    }
    if (mini_window_.isNull()) {
        LOG_MODULE("TrayController", "attach", LOG_WARN, "迷你窗为空，托盘菜单无法显示迷你窗");
    }
}

QStringList TrayController::menu_texts() const {
    QStringList texts;
    if (menu_ == nullptr) {
        return texts;
    }
    for (QAction* action : menu_->actions()) {
        if (!action->isSeparator()) {
            texts.append(action->text());
        }
    }
    return texts;
}

void TrayController::handleMainWindowClose() {
    if (available_ && !main_window_.isNull()) {
        main_window_->hide();
        LOG_MODULE("TrayController", "handleMainWindowClose", LOG_DEBUG, "主窗口已隐藏到托盘");
        return;
    }
    LOG_MODULE("TrayController", "handleMainWindowClose", LOG_INFO, "托盘不可用：主窗口关闭即退出应用");
    quit_application();
}

void TrayController::on_show_main_window() {
    show_window(main_window_);
}

void TrayController::on_show_mini_window() {
    show_window(mini_window_);
}

void TrayController::on_quit() {
    emit quitRequested();
    quit_application();
}

// 双击图标恢复主窗口；右键菜单由 setContextMenu 自行弹出
void TrayController::on_tray_activated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        show_window(main_window_);
    }
}

void TrayController::show_window(QQuickWindow* window) {
    if (window == nullptr) {
        LOG_MODULE("TrayController", "show_window", LOG_WARN, "目标窗口为空，无法显示");
        return;
    }
    if (window->windowState() & Qt::WindowMinimized) {
        window->showNormal();
    }
    else {
        window->show();
    }
    window->raise();
    window->requestActivate();
}

void TrayController::quit_application() {
    LOG_MODULE("TrayController", "quit_application", LOG_INFO, "退出应用：保存配置并结束事件循环");
    AppConfig::instance().save_all();
    if (tray_ != nullptr) {
        tray_->hide();
    }
    QCoreApplication::quit();
}
