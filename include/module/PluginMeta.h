/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventDefinition.h"

#include <string>
#include <vector>

// PluginMeta - 插件展示元数据（侧车 plugin.json 的解析结果）
// 侧车位于 <插件目录>/<动态库主干名>/plugin.json，头图等资源与该文件同目录；
// 所有字段均为可选，缺失即为空
struct PluginMeta {
    std::string display_name;            ///< 展示名称（JSON 字段 name）
    std::string author;                  ///< 作者
    std::string description;             ///< 一句话简介
    std::string banner;                  ///< 头图文件名（相对侧车目录）
    std::string version;                 ///< 插件版本
    std::vector<std::string> tags;       ///< 标签列表
    std::vector<EventDefinition> events; ///< 事件声明列表（缺失为空）
    bool found = false;                  ///< 侧车存在且解析成功
};

/// @brief 读取插件展示元数据侧车
/// @param plugin_dir 插件目录（动态库所在目录的绝对路径）
/// @param stem 动态库主干名（不含扩展名，同时也是侧车目录名）
/// @return 元数据；参数为空、侧车缺失、JSON 损坏或顶层非对象时返回空元数据（found 为 false）
PluginMeta read_plugin_meta(const std::string& plugin_dir, const std::string& stem);
