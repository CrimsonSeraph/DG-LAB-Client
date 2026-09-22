/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "AppBridge.h"

#include "AppConfig.h"

AppBridge::AppBridge(QObject* parent)
    : QObject(parent) {
}

void AppBridge::initialize() {
    const auto& config = AppConfig::instance();
    app_name_ = QString::fromStdString(config.get_value<std::string>("app.name", "DG-LAB-Client"));
    app_version_ = QString::fromStdString(config.get_value<std::string>("app.version", "2.0.0"));
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

void AppBridge::setStatus(const QString& text) {
    if (text == status_text_) {
        return;
    }
    status_text_ = text;
    emit statusTextChanged();
}
