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
    const std::vector<ModuleManager::PluginInfo> plugins = ModuleManager::instance().get_plugins();
    for (const ModuleManager::PluginInfo& plugin : plugins) {
        if (plugin.module_name.empty() || plugin.events.empty()) {
            continue;
        }
        std::map<std::string, EventConfig> configs = read_event_configs(plugin.module_name, plugin.events);
        // 运行期启用集合覆盖映射里的 enabled（不写回配置）
        apply_cascade_enabled(plugin.module_name, cascade_events_, configs);
        engine_.configure(plugin.module_name, plugin.events, configs);
    }
    LOG_MODULE("EventRuntime", "reload", LOG_DEBUG, "事件映射载入完成，模块数 " << plugins.size());
    emit reloaded();
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

const EventEngine& EventRuntime::engine() const {
    return engine_;
}

int EventRuntime::channel_output(const std::string& channel) const {
    return engine_.channel_output(channel);
}

void EventRuntime::on_frame_tick() {
    engine_.advance_frame(static_cast<std::int64_t>(QDateTime::currentMSecsSinceEpoch()));
    emit channelsChanged(channel_output("A"), channel_output("B"));
    emit triggersChanged();
}
