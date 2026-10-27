/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventBridge.h"

#include "DebugLog.h"
#include "EventEngine.h"
#include "EventMapping.h"
#include "EventRuntime.h"
#include "ModuleManager.h"
#include "ModuleValue.h"

#include <QDateTime>
#include <QVariantMap>

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

    constexpr int TRIGGER_LIMIT = 50; ///< 触发流水展示条数

    // 标准字符串转 QString（QML 直接消费）
    QString to_qstring(const std::string& text) {
        return QString::fromStdString(text);
    }

    // 比较方式转侧车契约文本（trigger.compare）
    QString compare_text(EventCompare compare) {
        switch (compare) {
        case EventCompare::INCREASE:
            return QStringLiteral("increase");
        case EventCompare::DECREASE:
            return QStringLiteral("decrease");
        case EventCompare::CROSS_ABOVE:
            return QStringLiteral("cross_above");
        case EventCompare::CROSS_BELOW:
            return QStringLiteral("cross_below");
        default:
            return QStringLiteral("any");
        }
    }

    // 通道配置转映射项
    QVariantMap channel_item(const EventChannelConfig& config) {
        QVariantMap item;
        item.insert(QStringLiteral("limit"), config.limit);
        item.insert(QStringLiteral("safety"), config.safety);
        return item;
    }

    // 子选项映射转展示项（label 由调用方按声明补齐，映射表独有键用键名）
    QVariantMap option_item(const std::string& option_id, const QString& label, const OptionOutput& output) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), to_qstring(option_id));
        item.insert(QStringLiteral("label"), label);
        item.insert(QStringLiteral("channel"), to_qstring(output.channel));
        item.insert(QStringLiteral("operation"), to_qstring(event_operation_to_string(output.operation)));
        item.insert(QStringLiteral("mode"), to_qstring(option_mode_to_string(output.mode)));
        item.insert(QStringLiteral("value"), output.value);
        item.insert(QStringLiteral("percent"), output.percent);
        item.insert(QStringLiteral("step"), output.step);
        item.insert(QStringLiteral("editedAt"), static_cast<qlonglong>(output.edited_at));
        return item;
    }

    // 触发数值的上下限与运行期值（数值未注册时各 Known 为 false，界面显示范围未知）
    void insert_value_range(QVariantMap& item, const std::string& module_name, const std::string& value_id) {
        const ModuleValue* value = ModuleManager::instance().get_value(module_name, value_id);
        const std::optional<int> min_value = value == nullptr ? std::nullopt : value->get_min();
        const std::optional<int> max_value = value == nullptr ? std::nullopt : value->get_max();
        item.insert(QStringLiteral("valueKnown"), value != nullptr);
        item.insert(QStringLiteral("valueMinKnown"), min_value.has_value());
        item.insert(QStringLiteral("valueMaxKnown"), max_value.has_value());
        item.insert(QStringLiteral("valueMin"), min_value.value_or(0));
        item.insert(QStringLiteral("valueMax"), max_value.value_or(0));
        item.insert(QStringLiteral("valueCurrentKnown"), value != nullptr && value->get_has_value());
        item.insert(QStringLiteral("valueCurrent"), value == nullptr ? 0 : value->get_last_value());
    }

    // 单个事件声明与当前映射合并为一项（cascade_events 为运行期启用集合，级联启用的事件同样显示为启用）
    QVariantMap event_item(const std::string& module_name, const EventDefinition& definition,
        const std::set<std::string>& cascade_events) {
        const EventConfig config = read_event_config(module_name, definition);

        QVariantMap trigger;
        trigger.insert(QStringLiteral("value"), to_qstring(definition.trigger.value_id));
        trigger.insert(QStringLiteral("compare"), compare_text(definition.trigger.compare));
        trigger.insert(QStringLiteral("amount"), definition.trigger.amount);

        QVariantList options;
        for (const auto& [option_id, output] : config.options) {
            if (option_id.empty()) {
                continue;
            }
            QString label = to_qstring(option_id);
            for (const auto& declared : definition.options) {
                if (declared.id != option_id) {
                    continue;
                }
                label = to_qstring(declared.label.empty() ? declared.id : declared.label);
                break;
            }
            options.append(option_item(option_id, label, output));
        }

        QVariantMap item;
        item.insert(QStringLiteral("id"), to_qstring(definition.id));
        item.insert(QStringLiteral("label"), to_qstring(definition.label.empty() ? definition.id : definition.label));
        item.insert(QStringLiteral("trigger"), trigger);
        item.insert(QStringLiteral("options"), options);
        const bool cascade_enabled = cascade_events.count(module_name + "/" + definition.id) != 0;
        item.insert(QStringLiteral("enabled"), config.enabled || cascade_enabled);
        item.insert(QStringLiteral("wave"), to_qstring(config.wave));
        item.insert(QStringLiteral("merge"), to_qstring(merge_rule_to_string(config.merge)));
        item.insert(QStringLiteral("weight"), config.weight);
        item.insert(QStringLiteral("frames"), config.frames);
        insert_value_range(item, module_name, definition.trigger.value_id);
        return item;
    }

    // 按模块标识取事件声明（无匹配返回空）
    std::vector<EventDefinition> module_events(const std::string& module_name) {
        for (const auto& plugin : ModuleManager::instance().get_plugins()) {
            if (plugin.module_name == module_name) {
                return plugin.events;
            }
        }
        return std::vector<EventDefinition>();
    }

} // namespace

EventBridge::EventBridge(EventRuntime* runtime, QObject* parent)
    : QObject(parent)
    , runtime_(runtime) {
}

void EventBridge::initialize() {
    if (runtime_ != nullptr) {
        connect(runtime_, &EventRuntime::channelsChanged, this, [this](int, int) { emit channelsChanged(); });
        connect(runtime_, &EventRuntime::triggersChanged, this, &EventBridge::triggersChanged);
        connect(runtime_, &EventRuntime::reloaded, this, &EventBridge::dataChanged);
    }
    connect(&ModuleManager::instance(), &ModuleManager::plugin_state_changed, this, &EventBridge::dataChanged);
    LOG_MODULE("EventBridge", "initialize", LOG_INFO, "事件桥接初始化完成");
}

QVariantList EventBridge::events() const {
    QVariantList list;
    const std::set<std::string> cascade_events = runtime_ != nullptr ? runtime_->cascade_events()
                                                                     : std::set<std::string>();
    for (const auto& plugin : ModuleManager::instance().get_plugins()) {
        if (plugin.module_name.empty() || plugin.events.empty()) {
            continue;
        }
        QVariantMap channels;
        channels.insert(QStringLiteral("A"), channel_item(read_channel_config(plugin.module_name, "A")));
        channels.insert(QStringLiteral("B"), channel_item(read_channel_config(plugin.module_name, "B")));

        QVariantList events;
        for (const auto& definition : plugin.events) {
            if (definition.id.empty()) {
                continue;
            }
            events.append(event_item(plugin.module_name, definition, cascade_events));
        }

        QVariantMap item;
        item.insert(QStringLiteral("module"), to_qstring(plugin.module_name));
        item.insert(QStringLiteral("displayName"),
            to_qstring(plugin.display_name.empty() ? plugin.module_name : plugin.display_name));
        item.insert(QStringLiteral("channels"), channels);
        item.insert(QStringLiteral("defaultChannel"), to_qstring(read_default_channel(plugin.module_name)));
        item.insert(QStringLiteral("events"), events);
        list.append(item);
    }
    return list;
}

QVariantList EventBridge::triggers() const {
    QVariantList list;
    if (runtime_ == nullptr) {
        return list;
    }
    for (const auto& record : runtime_->engine().recent_triggers(TRIGGER_LIMIT)) {
        QVariantMap item;
        item.insert(QStringLiteral("time"),
            QDateTime::fromMSecsSinceEpoch(record.timestamp_ms).toString(QStringLiteral("HH:mm:ss")));
        item.insert(QStringLiteral("timestampMs"), static_cast<qlonglong>(record.timestamp_ms));
        item.insert(QStringLiteral("module"), to_qstring(record.module_name));
        item.insert(QStringLiteral("event"), to_qstring(record.event_label.empty() ? record.event_id : record.event_label));
        item.insert(QStringLiteral("eventId"), to_qstring(record.event_id));
        item.insert(QStringLiteral("option"), to_qstring(record.option_label.empty() ? record.option_id : record.option_label));
        item.insert(QStringLiteral("optionId"), to_qstring(record.option_id));
        item.insert(QStringLiteral("channel"), to_qstring(record.channel));
        item.insert(QStringLiteral("operation"), to_qstring(record.operation));
        item.insert(QStringLiteral("delta"), record.delta);
        list.append(item);
    }
    return list;
}

int EventBridge::channel_a() const {
    return runtime_ == nullptr ? 0 : runtime_->channel_output("A");
}

int EventBridge::channel_b() const {
    return runtime_ == nullptr ? 0 : runtime_->channel_output("B");
}

void EventBridge::reload() {
    LOG_MODULE("EventBridge", "reload", LOG_DEBUG, "事件配置已重载");
    apply_change();
}

void EventBridge::setChannelConfig(const QString& module, const QString& channel, int limit, int safety) {
    const std::string module_name = module.toStdString();
    const std::string channel_name = channel.toStdString();
    if (module_name.empty() || channel_name.empty()) {
        return;
    }
    EventChannelConfig config;
    config.limit = limit;
    config.safety = safety;
    write_channel_config(module_name, channel_name, config);
    LOG_MODULE("EventBridge", "setChannelConfig", LOG_INFO,
        "通道配置已写入: 模块=" << module_name << " 通道=" << channel_name << " 上限=" << limit << " 安全上限=" << safety);
    apply_change();
}

void EventBridge::setDefaultChannel(const QString& module, const QString& channel) {
    const std::string module_name = module.toStdString();
    if (module_name.empty()) {
        return;
    }
    write_default_channel(module_name, channel.toStdString());
    LOG_MODULE("EventBridge", "setDefaultChannel", LOG_INFO,
        "默认输出通道已写入: 模块=" << module_name << " 通道=" << read_default_channel(module_name));
    apply_change();
}

void EventBridge::setEventConfig(const QString& module, const QString& event_id, bool enabled, const QString& wave,
    const QString& merge, double weight, int frames) {
    const std::string module_name = module.toStdString();
    const std::string target_event = event_id.toStdString();
    if (module_name.empty() || target_event.empty()) {
        return;
    }
    const std::vector<EventDefinition> definitions = module_events(module_name);
    const EventDefinition* definition = nullptr;
    for (const auto& candidate : definitions) {
        if (candidate.id == target_event) {
            definition = &candidate;
            break;
        }
    }
    EventConfig config;
    if (definition != nullptr) {
        config = read_event_config(module_name, *definition);
    }
    config.enabled = enabled;
    config.wave = wave.toStdString();
    config.merge = merge_rule_from_string(merge.toStdString());
    config.weight = weight;
    config.frames = frames;
    write_event_config(module_name, target_event, config);
    LOG_MODULE("EventBridge", "setEventConfig", LOG_INFO,
        "事件映射已写入: 模块=" << module_name << " 事件=" << target_event << " 启用=" << enabled);
    apply_change();
}

void EventBridge::setOptionOutput(const QString& module, const QString& event_id, const QString& option_id,
    const QString& channel, const QString& operation, const QString& mode, int value, int percent, int step) {
    const std::string module_name = module.toStdString();
    const std::string target_event = event_id.toStdString();
    const std::string target_option = option_id.toStdString();
    const std::string channel_name = channel.toStdString();
    if (module_name.empty() || target_event.empty() || target_option.empty()) {
        return;
    }
    // 空通道表示跟随模块默认通道，原样落盘
    OptionOutput output;
    output.channel = channel_name;
    output.operation = event_operation_from_string(operation.toStdString());
    output.mode = option_mode_from_string(mode.toStdString());
    output.value = value;
    output.percent = percent;
    output.step = step;
    write_option_output(module_name, target_event, target_option, output);
    LOG_MODULE("EventBridge", "setOptionOutput", LOG_INFO,
        "子选项映射已写入: 模块=" << module_name << " 事件=" << target_event << " 子选项=" << target_option
                                 << " 通道=" << channel_name);
    apply_change();
}

QVariantList EventBridge::resolveConflicts(const QString& module, const QString& channel) {
    QVariantList cleared;
    const std::string module_name = module.toStdString();
    const std::string channel_name = channel.toStdString();
    if (module_name.empty() || channel_name.empty()) {
        return cleared;
    }
    const std::vector<EventDefinition> definitions = module_events(module_name);
    if (definitions.empty()) {
        return cleared;
    }
    // 跟随默认（空 channel）的条目按模块默认通道展开后再判定互斥
    const std::string default_channel = read_default_channel(module_name);
    std::vector<OptionBinding> bindings = collect_option_bindings(read_event_configs(module_name, definitions));
    for (const std::string& display : apply_set_conflicts(module_name, channel_name, default_channel, bindings)) {
        cleared.append(to_qstring(display));
    }
    LOG_MODULE("EventBridge", "resolveConflicts", LOG_INFO,
        "互斥清理完成: 模块=" << module_name << " 通道=" << channel_name << " 默认通道=" << default_channel
                             << " 清理数=" << cleared.size());
    apply_change();
    return cleared;
}

void EventBridge::apply_change() {
    if (runtime_ == nullptr) {
        emit dataChanged();
        return;
    }
    runtime_->reload();
}
