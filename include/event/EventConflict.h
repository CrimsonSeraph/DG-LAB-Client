/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventMapping.h"

#include <string>
#include <vector>

// 子选项的「设为」互斥检测与清理（纯计算，不读写配置）

/// @brief 通道标识归一为大写
/// @param channel 通道文本（大小写不敏感）
/// @return 归一后的通道文本（"a" -> "A"）
std::string normalize_channel(const std::string& channel);

/// @brief 子选项标识转展示文本
/// @param key 子选项标识
/// @return "事件id/子选项id"
std::string option_key_display(const OptionKey& key);

/// @brief 检测同一通道上的「设为」互斥
/// @param channel 通道（"A"/"B"）
/// @param bindings 子选项条目列表
/// @return 该通道上已启用且操作为 set 的条目
std::vector<OptionBinding> detect_set_conflicts(const std::string& channel,
    const std::vector<OptionBinding>& bindings);

/// @brief 清理同一通道上的「设为」互斥（纯计算）
/// @param channel 通道（"A"/"B"）
/// @param bindings 子选项条目列表，被关闭项置为 enabled=false
/// @return 被关闭项的展示标识列表
/// @note 保留 edited_at 最大者，相同时保留标识字典序较小者
/// @note 不读写配置文件；需要落盘删除时用 apply_set_conflicts
std::vector<std::string> resolve_set_conflicts(const std::string& channel,
    std::vector<OptionBinding>& bindings);
