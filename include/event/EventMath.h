/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventMapping.h"

#include <QString>

#include <vector>

// 事件引擎的纯函数：强度钳制、合并规则、数值归一、波形包络与通道输出合并

/// @brief 强度钳制
/// @param value 原始强度
/// @param limit 通道上限
/// @param safety 通道安全上限
/// @return 落在 [0, min(limit, safety)] 的整数强度
int clamp_strength(int value, int limit, int safety);

/// @brief 合并规则计算
/// @param rule 合并规则
/// @param values 各路结果（已按通道上限归一为 0~1 的比例），空列表返回 0
/// @param weights 各路权重，数量不足时缺省取 1
/// @return 合并后的比例（0~1）
/// @note multiply 为各路连乘；max / min 取极值；weighted 为 Σ(w·v)/Σw，Σw=0 时退化为算术平均
double apply_merge(MergeRule rule, const std::vector<double>& values, const std::vector<double>& weights);

/// @brief 数值按上下限线性归一
/// @param value 原始数值
/// @param min_value 最小值
/// @param max_value 最大值
/// @return 0~1 的比例；max_value 不大于 min_value 时返回 0
double normalize_value(int value, int min_value, int max_value);

/// @brief 波形帧强度包络
/// @param frame 8 字节 HEX 帧（前 4 字节频率，后 4 字节强度 0~100）
/// @return 后 4 个强度字节均值（0~100），非法帧返回 0
int wave_envelope_percent(const QString& frame);

// 通道输出合并状态：合并规则命令最近值与事件输出最近值，按通道上限钳制并按需下发
struct ChannelOutputState {
    int rule_value = 0;   ///< 规则命令最近值
    int event_value = 0;  ///< 事件输出最近值
    int last_sent = -1;   ///< 上一次下发值（-1 表示与任何强度都不同）

    /// @brief 合并后的通道强度（规则与事件取较大值，按通道上限钳制）
    /// @param limit 通道上限
    /// @return 落在 [0, limit] 的强度
    int final_value(int limit) const;

    /// @brief 取需要下发的强度（与上一次下发值相同时不下发）
    /// @param limit 通道上限
    /// @param value 输出：需要下发的强度
    /// @return 需要下发返回 true
    bool take_output(int limit, int& value);

    /// @brief 复位上一次下发值（无输出目标时使用，连接后按当前强度重发）
    void reset_output();
};
