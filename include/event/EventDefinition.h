/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonObject>

#include <string>
#include <vector>

// 事件比较方式（侧车 trigger.compare）
enum class EventCompare {
    ANY = 0,         ///< 任意变化（忽略 amount）
    INCREASE = 1,    ///< 数值增加
    DECREASE = 2,    ///< 数值减少
    CROSS_ABOVE = 3, ///< 向上跨越阈值
    CROSS_BELOW = 4  ///< 向下跨越阈值
};

/// @brief 事件触发条件（引用插件已注册的数值）
struct EventTrigger {
    std::string value_id;                     ///< 数值 ID（侧车 trigger.value）
    EventCompare compare = EventCompare::ANY; ///< 比较方式
    int amount = 0;                           ///< 阈值/步长（compare=any 时忽略）
};

/// @brief 事件子选项（如礼物类型）
struct EventOption {
    std::string id;    ///< 子选项标识
    std::string label; ///< 展示名称
};

/// @brief 事件声明（侧车 events 数组的一项）
struct EventDefinition {
    std::string id;                   ///< 事件标识（用户映射键使用）
    std::string label;                ///< 展示名称
    EventTrigger trigger;             ///< 触发条件
    std::vector<EventOption> options; ///< 子选项列表（可为空）
};

/// @brief 比较方式文本转枚举
/// @param text 侧车中的 compare 文本
/// @return 对应枚举，未知文本返回 EventCompare::ANY
EventCompare event_compare_from_string(const std::string& text);

/// @brief 解析单个事件声明
/// @param object 事件 JSON 对象
/// @return 事件声明；id 或 trigger.value 缺失、类型不符时返回空声明（id 为空）
EventDefinition parse_event_definition(const QJsonObject& object);

/// @brief 解析侧车的全部事件声明
/// @param sidecar 侧车顶层 JSON 对象
/// @return 事件声明列表；events 缺失、非数组时为空，非法项与重复 id 被跳过
std::vector<EventDefinition> parse_event_definitions(const QJsonObject& sidecar);
