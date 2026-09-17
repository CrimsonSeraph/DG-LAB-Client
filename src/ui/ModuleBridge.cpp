/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ModuleBridge.h"

#include "DebugLog.h"
#include "ModuleManager.h"

#include <QSet>
#include <QVariantMap>

#include <iterator>
#include <utility>

namespace {

    /// @brief 周期选项（文本 -> 毫秒），与 QueryPeriod 对应
    const std::pair<const char*, int> kPeriodOptions[] = {
        { "四分之一秒", 250 },
        { "每半秒", 500 },
        { "每秒", 1000 },
        { "每两秒", 2000 },
        { "每四秒", 4000 }
    };

} // namespace

ModuleBridge::ModuleBridge(QObject* parent)
    : QObject(parent) {
}

void ModuleBridge::initialize() {
    auto& manager = ModuleManager::instance();
    connect(&manager, &ModuleManager::value_changed, this, [this](const QString&, const QString&, int) {
        emit selectedChanged();
        emit dataChanged();
    });
    connect(&manager, &ModuleManager::period_changed, this, &ModuleBridge::dataChanged);
    connect(&manager, &ModuleManager::plugin_state_changed, this, &ModuleBridge::dataChanged);
    LOG_MODULE("ModuleBridge", "initialize", LOG_INFO, "模块桥接初始化完成");
}

QVariantList ModuleBridge::modules() const {
    QVariantList list;
    auto& manager = ModuleManager::instance();
    QSet<QString> loaded_names;

    for (const auto& plugin : manager.get_plugins()) {
        const QString name = QString::fromStdString(plugin.display_name);
        QVariantMap item;
        item.insert(QStringLiteral("kind"), QStringLiteral("plugin"));
        item.insert(QStringLiteral("fileName"), QString::fromStdString(plugin.file_name));
        item.insert(QStringLiteral("name"), name);
        item.insert(QStringLiteral("version"), QString::fromStdString(plugin.version));
        item.insert(QStringLiteral("error"), QString::fromStdString(plugin.error));

        QString state_text;
        bool enabled = false;
        switch (plugin.state) {
        case ModuleManager::PluginLoadState::Loaded:
            state_text = QStringLiteral("已加载");
            enabled = true;
            loaded_names.insert(name);
            break;
        case ModuleManager::PluginLoadState::Failed:
            state_text = QStringLiteral("加载失败");
            break;
        default:
            state_text = QStringLiteral("暂未加载");
            break;
        }
        item.insert(QStringLiteral("stateText"), state_text);
        item.insert(QStringLiteral("enabled"), enabled);
        item.insert(QStringLiteral("periodMs"), enabled ? manager.get_module_min_period_ms(plugin.display_name) : 0);
        list.append(item);
    }

    for (const auto& raw_name : manager.get_module_names()) {
        const QString name = QString::fromStdString(raw_name);
        if (loaded_names.contains(name)) {
            continue;
        }
        QVariantMap item;
        item.insert(QStringLiteral("kind"), QStringLiteral("module"));
        item.insert(QStringLiteral("fileName"), QString());
        item.insert(QStringLiteral("name"), name);
        item.insert(QStringLiteral("version"), QString());
        item.insert(QStringLiteral("error"), QString());
        item.insert(QStringLiteral("stateText"), QStringLiteral("内置模块"));
        item.insert(QStringLiteral("enabled"), true);
        item.insert(QStringLiteral("periodMs"), manager.get_module_min_period_ms(raw_name));
        list.append(item);
    }
    return list;
}

QVariantList ModuleBridge::period_options() const {
    QVariantList list;
    for (const auto& option : kPeriodOptions) {
        QVariantMap item;
        item.insert(QStringLiteral("text"), QString::fromUtf8(option.first));
        item.insert(QStringLiteral("ms"), option.second);
        list.append(item);
    }
    return list;
}

int ModuleBridge::base_period_ms() const {
    return ModuleManager::instance().get_base_period_ms();
}

QVariantList ModuleBridge::selected_values() const {
    QVariantList list;
    if (selected_.isEmpty()) {
        return list;
    }
    const Module* module = ModuleManager::instance().get_module(selected_.toStdString());
    if (module == nullptr) {
        return list;
    }
    for (const auto& value : module->get_values()) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QString::fromStdString(value.get_id()));
        item.insert(QStringLiteral("name"), QString::fromStdString(value.get_name()));
        item.insert(QStringLiteral("value"), value.get_has_value() ? QVariant(value.get_last_value()) : QVariant());
        item.insert(QStringLiteral("min"), value.get_min().has_value() ? QVariant(value.get_min().value()) : QVariant());
        item.insert(QStringLiteral("max"), value.get_max().has_value() ? QVariant(value.get_max().value()) : QVariant());
        item.insert(QStringLiteral("periodText"), QString::fromUtf8(query_period_to_text(value.get_query_period())));
        list.append(item);
    }
    return list;
}

void ModuleBridge::applyAllPeriod(int index) {
    if (index < 0 || index >= static_cast<int>(std::size(kPeriodOptions))) {
        return;
    }
    const int ms = kPeriodOptions[index].second;
    ModuleManager::instance().set_all_period(query_period_from_ms(ms));
    LOG_MODULE("ModuleBridge", "applyAllPeriod", LOG_INFO, "统一查询周期已设置");
    emit dataChanged();
}

void ModuleBridge::togglePlugin(const QString& file_name) {
    if (file_name.isEmpty()) {
        return;
    }
    auto& manager = ModuleManager::instance();
    for (const auto& plugin : manager.get_plugins()) {
        if (QString::fromStdString(plugin.file_name) != file_name) {
            continue;
        }
        if (plugin.state == ModuleManager::PluginLoadState::Loaded) {
            manager.unload_plugin(plugin.file_name);
        }
        else {
            manager.load_plugin(plugin.file_name);
        }
        break;
    }
    emit dataChanged();
}

void ModuleBridge::selectModule(const QString& module_name) {
    selected_ = module_name;
    emit selectedChanged();
}
