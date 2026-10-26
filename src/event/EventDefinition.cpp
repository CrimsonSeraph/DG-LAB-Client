/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventDefinition.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <string>
#include <utility>
#include <vector>

namespace {

    // 读取字符串字段（缺失或类型不符返回空串）
    std::string read_string_field(const QJsonObject& object, const char* key) {
        const QJsonValue value = object.value(QString::fromUtf8(key));
        if (!value.isString()) {
            return std::string();
        }
        return value.toString().toStdString();
    }

    // 解析子选项数组（缺失、非数组返回空列表，非法项跳过）
    std::vector<EventOption> parse_options(const QJsonValue& value) {
        std::vector<EventOption> options;
        if (!value.isArray()) {
            return options;
        }
        const QJsonArray array = value.toArray();
        for (int index = 0; index < array.size(); ++index) {
            const QJsonValue item = array.at(index);
            if (!item.isObject()) {
                continue;
            }
            const QJsonObject object = item.toObject();
            EventOption option;
            option.id = read_string_field(object, "id");
            if (option.id.empty()) {
                continue;
            }
            option.label = read_string_field(object, "label");
            options.push_back(std::move(option));
        }
        return options;
    }

    // 判断列表中是否已存在同名事件
    bool has_event_id(const std::vector<EventDefinition>& definitions, const std::string& id) {
        for (const auto& definition : definitions) {
            if (definition.id == id) {
                return true;
            }
        }
        return false;
    }

} // namespace

EventCompare event_compare_from_string(const std::string& text) {
    if (text == "increase") {
        return EventCompare::INCREASE;
    }
    if (text == "decrease") {
        return EventCompare::DECREASE;
    }
    if (text == "cross_above") {
        return EventCompare::CROSS_ABOVE;
    }
    if (text == "cross_below") {
        return EventCompare::CROSS_BELOW;
    }
    return EventCompare::ANY;
}

EventDefinition parse_event_definition(const QJsonObject& object) {
    EventDefinition definition;
    definition.id = read_string_field(object, "id");
    if (definition.id.empty()) {
        return EventDefinition();
    }
    const QJsonValue trigger_value = object.value(QStringLiteral("trigger"));
    if (!trigger_value.isObject()) {
        return EventDefinition();
    }
    const QJsonObject trigger_object = trigger_value.toObject();
    EventTrigger trigger;
    trigger.value_id = read_string_field(trigger_object, "value");
    if (trigger.value_id.empty()) {
        return EventDefinition();
    }
    trigger.compare = event_compare_from_string(read_string_field(trigger_object, "compare"));
    const QJsonValue amount_value = trigger_object.value(QStringLiteral("amount"));
    if (amount_value.isDouble()) {
        trigger.amount = amount_value.toInt();
    }
    definition.label = read_string_field(object, "label");
    definition.trigger = trigger;
    definition.options = parse_options(object.value(QStringLiteral("options")));
    return definition;
}

std::vector<EventDefinition> parse_event_definitions(const QJsonObject& sidecar) {
    std::vector<EventDefinition> definitions;
    const QJsonValue events_value = sidecar.value(QStringLiteral("events"));
    if (!events_value.isArray()) {
        return definitions;
    }
    const QJsonArray array = events_value.toArray();
    for (int index = 0; index < array.size(); ++index) {
        const QJsonValue item = array.at(index);
        if (!item.isObject()) {
            continue;
        }
        EventDefinition definition = parse_event_definition(item.toObject());
        if (definition.id.empty() || has_event_id(definitions, definition.id)) {
            continue;
        }
        definitions.push_back(std::move(definition));
    }
    return definitions;
}
