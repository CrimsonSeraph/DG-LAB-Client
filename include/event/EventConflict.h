/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventMapping.h"

#include <string>
#include <vector>

// 子选项的「设为」互斥检测与清理（纯计算，不读写配置）
// 判定基于条目的「实际生效通道」：存储 channel 为空串时按模块默认通道展开（AB 展开为 A 与 B）

/// @brief 通道标识归一为大写
/// @param channel 通道文本（大小写不敏感）
/// @return 归一后的通道文本（"a" -> "A"）
std::string normalize_channel(const std::string& channel);

/// @brief 子选项标识转展示文本
/// @param key 子选项标识
/// @return "事件id/子选项id"
std::string option_key_display(const OptionKey& key);

/// @brief 互斥落败条目
/// @note 存储层每个子选项只有一条配置、无法按通道拆分，因此落败即整条关闭；
///       channels 记录它关闭前占用的实际通道（"A"/"B"），供界面说明影响范围
struct SetConflictLoss {
    OptionKey key;                     ///< 归属标识
    std::vector<std::string> channels; ///< 关闭前实际生效的通道（按 A→B 排序）
};

/// @brief 检测指定通道上的「设为」互斥
/// @param channel 请求判定的通道（"A"/"B"）
/// @param default_channel 模块默认通道（"A"/"B"/"AB"；存储为空串的条目按它展开）
/// @param bindings 子选项条目列表
/// @return 该通道上已启用且实际生效通道包含它的 set 条目
std::vector<OptionBinding> detect_set_conflicts(const std::string& channel, const std::string& default_channel,
    const std::vector<OptionBinding>& bindings);

/// @brief 清理指定通道上的「设为」互斥（纯计算）
/// @param channel 请求判定的通道（"A"/"B"）
/// @param default_channel 模块默认通道（"A"/"B"/"AB"；存储为空串的条目按它展开）
/// @param bindings 子选项条目列表，被关闭项置为 enabled=false
/// @return 被关闭条目及其关闭前占用的通道
/// @note 保留 edited_at 最大者，相同时保留标识字典序较小者
/// @note 不读写配置文件；需要落盘删除时用 apply_set_conflicts
std::vector<SetConflictLoss> resolve_set_conflicts(const std::string& channel, const std::string& default_channel,
    std::vector<OptionBinding>& bindings);
