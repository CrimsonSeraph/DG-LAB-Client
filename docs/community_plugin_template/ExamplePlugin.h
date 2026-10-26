/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "PluginHost.h"

#include <string>

// ExamplePlugin - 社区插件模板
// 演示 IPlugin 接口实现、数值注册与动态库导出约定（无实际功能）；
// 复制整个模板目录到 module/<插件名>/ 后按需改写类名与数值
class ExamplePlugin : public IPlugin {
public:
    // 自描述
    /// @brief 获取插件名称（同时作为模块标识，供规则引用与模块查询）
    /// @return 模块标识（ASCII；展示名放侧车 plugin.json 的 name 字段）
    std::string name() const override { return "example"; }

    /// @brief 获取插件版本号（与侧车 plugin.json 的 version 保持一致）
    /// @return 版本号
    std::string version() const override { return "0.1.0"; }

    /// @brief 获取插件 API 版本（宿主加载时与 PLUGIN_API_VERSION 校验）
    /// @return API 版本号
    int api_version() const override { return PLUGIN_API_VERSION; }

    /// @brief 获取插件能力标志（提供可查询数值）
    /// @return 能力标志
    PluginCapability capabilities() const override { return PluginCapability::ProvidesValues; }

    // 生命周期
    /// @brief 初始化：注册数值并读取宿主配置
    /// @return 成功返回 PluginError::Ok
    PluginError initialize() override;

    /// @brief 反初始化：注销模块（与 initialize 对称）
    void uninitialize() override;

    /// @brief 卸载前检查：无活动资源，始终允许卸载
    /// @return 允许卸载返回 true
    bool can_unload() const override { return true; }
};
