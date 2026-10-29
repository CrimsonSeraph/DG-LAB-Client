/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventRuntime.h"

#include "DebugLog.h"
#include "EventMapping.h"
#include "ModuleManager.h"
#include "RuleManager.h"

#include <QDateTime>
#include <QTimer>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

    /// @brief 列表转运行期启用集合
    std::set<std::string> to_event_set(const std::vector<std::string>& events) {
        return std::set<std::string>(events.begin(), events.end());
    }

} // namespace

EventRuntime::EventRuntime(QObject* parent)
    : QObject(parent) {
}

void EventRuntime::initialize() {
    if (frame_timer_ != nullptr) {
        return;
    }
    ModuleManager& modules = ModuleManager::instance();
    connect(&modules, &ModuleManager::value_changed, this,
        [this](const QString& module_name, const QString& value_id, int value) {
            engine_.on_value_changed(module_name.toStdString(), value_id.toStdString(), value);
        });
    // 插件卸载：立即丢弃该模块的事件声明与运行期状态，不等下一次全量重建
    connect(&modules, &ModuleManager::module_unregistered, this, [this](const QString& module_name) {
        engine_.remove_module(module_name.toStdString());
        LOG_MODULE("EventRuntime", "module_unregistered", LOG_INFO,
            "已清理模块的事件声明与运行期状态: " << module_name.toStdString());
    });
    connect(&modules, &ModuleManager::plugin_state_changed, this, &EventRuntime::reload);

    // 启用级联：规则层的依赖集合决定哪些事件在运行期视为启用（初始化拉取一次，此后随信号刷新）
    cascade_events_ = to_event_set(RuleManager::instance().get_cascade_enabled_events());
    connect(&RuleManager::instance(), &RuleManager::cascade_changed, this, [this]() {
        set_cascade_events(to_event_set(RuleManager::instance().get_cascade_enabled_events()));
    });

    frame_timer_ = new QTimer(this);
    frame_timer_->setTimerType(Qt::PreciseTimer);
    frame_timer_->setInterval(FRAME_INTERVAL_MS);
    connect(frame_timer_, &QTimer::timeout, this, &EventRuntime::on_frame_tick);
    frame_timer_->start();

    reload();
    LOG_MODULE("EventRuntime", "initialize", LOG_INFO, "事件运行时初始化完成");
}

void EventRuntime::reload() {
    engine_.clear();
    std::set<std::string> plugin_modules;
    const std::vector<ModuleManager::PluginInfo> plugins = ModuleManager::instance().get_plugins();
    for (const ModuleManager::PluginInfo& plugin : plugins) {
        if (plugin.module_name.empty() || plugin.events.empty()) {
            continue;
        }
        configure_module(plugin.module_name, plugin.events);
        plugin_modules.insert(plugin.module_name);
    }
    // 内置设备声明是第二个来源：同一模块标识已由插件声明事件时以插件为准，避免两套声明互相覆盖
    int builtin_modules = 0;
    for (const auto& [module_name, definitions] : builtin_events_) {
        if (definitions.empty() || plugin_modules.count(module_name) != 0) {
            continue;
        }
        configure_module(module_name, definitions);
        ++builtin_modules;
    }
    LOG_MODULE("EventRuntime", "reload", LOG_DEBUG,
        "事件映射载入完成，插件模块数 " << plugin_modules.size() << " 内置模块数 " << builtin_modules);
    emit reloaded();
}

void EventRuntime::register_builtin_events(const std::string& module_name,
    const std::vector<EventDefinition>& events) {
    if (module_name.empty()) {
        return;
    }
    if (events.empty()) {
        // 空声明等于清除该模块的内置来源；插件仍在声明该模块时不动它的运行期状态
        builtin_events_.erase(module_name);
        bool plugin_declared = false;
        for (const ModuleManager::PluginInfo& plugin : ModuleManager::instance().get_plugins()) {
            if (plugin.module_name == module_name && !plugin.events.empty()) {
                plugin_declared = true;
                break;
            }
        }
        if (!plugin_declared) {
            engine_.remove_module(module_name);
        }
    }
    else {
        // 键为模块标识：重复注册整体替换，不会累积出重复声明
        builtin_events_[module_name] = events;
    }
    LOG_MODULE("EventRuntime", "register_builtin_events", LOG_INFO,
        "内置事件声明已注册: " << module_name << "，事件数 " << events.size());
    if (frame_timer_ != nullptr) {
        reload();
    }
}

void EventRuntime::unregister_builtin_events(const std::string& module_name) {
    if (module_name.empty()) {
        return;
    }
    const bool erased = builtin_events_.erase(module_name) > 0;
    // 插件仍在声明同名模块时不丢它的运行期状态（与注册路径的「插件优先」保持一致）
    bool plugin_declared = false;
    for (const ModuleManager::PluginInfo& plugin : ModuleManager::instance().get_plugins()) {
        if (plugin.module_name == module_name && !plugin.events.empty()) {
            plugin_declared = true;
            break;
        }
    }
    if (!plugin_declared) {
        engine_.remove_module(module_name);
    }
    LOG_MODULE("EventRuntime", "unregister_builtin_events", LOG_INFO,
        "内置事件声明已注销: " << module_name << "（声明来源" << (erased ? "已移除" : "本就不存在") << "）");
    // 声明仍留在引擎里的模块（插件声明）需要重载一次才能与注册表对齐
    if (frame_timer_ != nullptr && !plugin_declared) {
        reload();
    }
}

const EventEngine& EventRuntime::engine() const {
    return engine_;
}

void EventRuntime::set_cascade_events(const std::set<std::string>& events) {
    if (cascade_events_ == events) {
        return;
    }
    cascade_events_ = events;
    LOG_MODULE("EventRuntime", "set_cascade_events", LOG_INFO,
        "运行期启用集合已更新，事件数 " << cascade_events_.size());
    // 映射已载入：立即重建，下一次帧推进生效
    reload();
}

int EventRuntime::channel_output(const std::string& channel) const {
    return engine_.channel_output(channel);
}

void EventRuntime::configure_module(const std::string& module_name,
    const std::vector<EventDefinition>& definitions) {
    std::map<std::string, EventConfig> configs = read_event_configs(module_name, definitions);
    // 运行期启用集合覆盖映射里的 enabled（不写回配置）
    apply_cascade_enabled(module_name, cascade_events_, configs);
    engine_.configure(module_name, definitions, configs);
}

void EventRuntime::on_frame_tick() {
    engine_.advance_frame(static_cast<std::int64_t>(QDateTime::currentMSecsSinceEpoch()));
    emit channelsChanged(channel_output("A"), channel_output("B"));
    emit triggersChanged();
}
