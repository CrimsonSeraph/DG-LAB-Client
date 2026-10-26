/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventRuntime.h"

#include "DebugLog.h"
#include "EventMapping.h"
#include "ModuleManager.h"

#include <QDateTime>
#include <QTimer>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

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
        const std::map<std::string, EventConfig> configs = read_event_configs(plugin.module_name, plugin.events);
        engine_.configure(plugin.module_name, plugin.events, configs);
    }
    LOG_MODULE("EventRuntime", "reload", LOG_DEBUG, "事件映射载入完成，模块数 " << plugins.size());
    emit reloaded();
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
