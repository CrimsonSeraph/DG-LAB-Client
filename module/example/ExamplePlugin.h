/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "IPlugin.h"

#include <string>

// ============================================
// ExamplePlugin - 示例空壳插件
// 演示 IPlugin 接口实现与动态库导出约定（无实际功能），
// 用于验证插件扫描/加载/卸载流程
// ============================================
class ExamplePlugin : public IPlugin {
public:
    // -------------------- 自描述 --------------------
    /// @brief 获取插件名称
    /// @return 插件名称
    std::string name() const override { return "示例插件"; }

    /// @brief 获取插件版本号
    /// @return 版本号
    std::string version() const override { return "0.1.0"; }

    /// @brief 获取插件 API 版本
    /// @return API 版本号
    int api_version() const override { return PLUGIN_API_VERSION; }

    /// @brief 获取插件能力标志（提供可查询数值）
    /// @return 能力标志
    PluginCapability capabilities() const override { return PluginCapability::ProvidesValues; }

    // -------------------- 生命周期 --------------------
    /// @brief 初始化：通过宿主上下文注册示例数值并输出日志
    /// @return 成功返回 PluginError::Ok
    PluginError initialize() override;

    /// @brief 反初始化
    void uninitialize() override;

    /// @brief 卸载前检查：无活动资源，始终允许卸载
    /// @return 允许卸载返回 true
    bool can_unload() const override;
};
