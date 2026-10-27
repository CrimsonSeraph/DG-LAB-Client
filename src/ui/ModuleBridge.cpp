/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ModuleBridge.h"

#include "DebugLog.h"
#include "ModuleManager.h"

#include <QSet>
#include <QStringList>
#include <QVariantMap>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

    /// @brief 周期选项（文本 -> 毫秒），与 QueryPeriod 对应；文本直接用毫秒数值，避免口语量词
    const std::pair<const char*, int> kPeriodOptions[] = {
        { "250 ms", 250 },
        { "500 ms", 500 },
        { "1000 ms", 1000 },
        { "2000 ms", 2000 },
        { "4000 ms", 4000 }
    };

    // 标签列表转换为 QStringList（QML 直接消费）
    QStringList to_qstring_list(const std::vector<std::string>& values) {
        QStringList list;
        for (const auto& value : values) {
            list.append(QString::fromStdString(value));
        }
        return list;
    }

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
        const QString module_name = QString::fromStdString(plugin.module_name);
        QVariantMap item;
        item.insert(QStringLiteral("kind"), QStringLiteral("plugin"));
        item.insert(QStringLiteral("fileName"), QString::fromStdString(plugin.file_name));
        item.insert(QStringLiteral("name"), name);
        item.insert(QStringLiteral("version"), QString::fromStdString(plugin.version));
        item.insert(QStringLiteral("error"), QString::fromStdString(plugin.error));
        item.insert(QStringLiteral("banner"), QString::fromStdString(plugin.banner));
        item.insert(QStringLiteral("author"), QString::fromStdString(plugin.author));
        item.insert(QStringLiteral("description"), QString::fromStdString(plugin.description));
        item.insert(QStringLiteral("tags"), to_qstring_list(plugin.tags));
        item.insert(QStringLiteral("moduleName"), module_name);

        QString state_text;
        QString state;
        bool enabled = false;
        switch (plugin.state) {
        case ModuleManager::PluginLoadState::Loaded:
            state_text = QStringLiteral("已加载");
            state = QStringLiteral("loaded");
            enabled = true;
            // 去重按模块标识，displayName 只用于展示
            if (!module_name.isEmpty()) {
                loaded_names.insert(module_name);
            }
            break;
        case ModuleManager::PluginLoadState::Failed:
            state_text = QStringLiteral("加载失败");
            state = QStringLiteral("failed");
            break;
        default:
            state_text = QStringLiteral("暂未加载");
            state = QStringLiteral("notLoaded");
            break;
        }
        item.insert(QStringLiteral("state"), state);
        item.insert(QStringLiteral("stateText"), state_text);
        item.insert(QStringLiteral("enabled"), enabled);
        int period_ms = 0;
        if (enabled && !module_name.isEmpty()) {
            period_ms = manager.get_module_min_period_ms(plugin.module_name);
        }
        item.insert(QStringLiteral("periodMs"), period_ms);
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
        item.insert(QStringLiteral("moduleName"), name);
        item.insert(QStringLiteral("version"), QString());
        item.insert(QStringLiteral("error"), QString());
        item.insert(QStringLiteral("state"), QStringLiteral("builtin"));
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

int ModuleBridge::uniform_period_index() const {
    auto& manager = ModuleManager::instance();
    int uniform_ms = -1;
    for (const auto& name : manager.get_module_names()) {
        const Module* module = manager.get_module(name);
        if (module == nullptr) {
            continue;
        }
        for (const auto& value : module->get_values()) {
            const int ms = query_period_to_ms(value.get_query_period());
            if (uniform_ms < 0) {
                uniform_ms = ms;
            }
            else if (ms != uniform_ms) {
                return -1;
            }
        }
    }
    if (uniform_ms < 0) {
        // 未注册任何数值时按基准周期回显，避免下拉空置
        uniform_ms = manager.get_base_period_ms();
    }
    for (int index = 0; index < static_cast<int>(std::size(kPeriodOptions)); ++index) {
        if (kPeriodOptions[index].second == uniform_ms) {
            return index;
        }
    }
    return -1;
}

QString ModuleBridge::selected_display_name() const {
    if (selected_.isEmpty()) {
        return selected_;
    }
    const std::string selected = selected_.toStdString();
    for (const auto& plugin : ModuleManager::instance().get_plugins()) {
        if (plugin.module_name != selected || plugin.display_name.empty()) {
            continue;
        }
        return QString::fromStdString(plugin.display_name);
    }
    // 内置模块与未匹配场景回退模块标识，行为与旧版一致
    return selected_;
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
    bool found = false;
    bool was_loaded = false;
    for (const auto& plugin : manager.get_plugins()) {
        if (QString::fromStdString(plugin.file_name) != file_name) {
            continue;
        }
        found = true;
        was_loaded = plugin.state == ModuleManager::PluginLoadState::Loaded;
        break;
    }
    if (!found) {
        emit pluginToggled(file_name, false, QStringLiteral("插件不存在：") + file_name);
        emit dataChanged();
        return;
    }

    const bool ok = was_loaded ? manager.unload_plugin(file_name.toStdString())
                               : manager.load_plugin(file_name.toStdString());
    QString error;
    if (!ok) {
        // 失败原因以管理器记录的加载错误为准；卸载被插件拒绝时没有 error 字段
        for (const auto& plugin : manager.get_plugins()) {
            if (QString::fromStdString(plugin.file_name) == file_name) {
                error = QString::fromStdString(plugin.error);
                break;
            }
        }
        if (error.isEmpty()) {
            error = was_loaded ? QStringLiteral("插件拒绝卸载，仍处于启用状态")
                               : QStringLiteral("插件加载失败");
        }
    }
    emit pluginToggled(file_name, ok, error);
    emit dataChanged();
}

void ModuleBridge::selectModule(const QString& module_name) {
    selected_ = module_name;
    emit selectedChanged();
}
