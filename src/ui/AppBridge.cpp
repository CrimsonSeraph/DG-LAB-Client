/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "AppBridge.h"

#include "AppConfig.h"

namespace {
/// @brief 一次性反馈浮层的驻留时长
constexpr int toast_duration_ms = 3000;
} // namespace

AppBridge::AppBridge(QObject* parent)
    : QObject(parent) {
    toast_timer_.setSingleShot(true);
    toast_timer_.setInterval(toast_duration_ms);
    // 定时器到期只清空文本：Toast 是否可见由 QML 绑定 toastText.length 决定
    QObject::connect(&toast_timer_, &QTimer::timeout, this, [this]() {
        toast_text_.clear();
        emit toastChanged();
    });
}

void AppBridge::initialize() {
    const auto& config = AppConfig::instance();
    app_name_ = QString::fromStdString(config.get_value<std::string>("app.name", "DG-LAB-Client"));
    app_version_ = QString::fromStdString(config.get_value<std::string>("app.version", "2.2.0"));
    emit appInfoChanged();
}

void AppBridge::set_current_page(int page) {
    if (page < 0 || page >= page_count() || page == current_page_) {
        return;
    }
    current_page_ = page;
    emit currentPageChanged();
}

void AppBridge::navigate(int page) {
    set_current_page(page);
}

void AppBridge::showToast(const QString& text, int level) {
    toast_text_ = text;
    toast_level_ = level;
    // 空文本立即隐藏；非空则重新起算 3 秒（连续消息只续期，不叠加多个倒计时）
    if (toast_text_.isEmpty()) {
        toast_timer_.stop();
    }
    else {
        toast_timer_.start();
    }
    emit toastChanged();
}

void AppBridge::setStatus(const QString& text, int level) {
    showToast(text, level);
}
