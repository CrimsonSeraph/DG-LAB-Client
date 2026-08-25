/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "IPlugin.h"
#include "ModuleValue.h"

#include <string>
#include <vector>

// 前置声明
class DataListener;

// ============================================
// IPluginHost - 宿主上下文接口
// 宿主在加载插件后通过 IPlugin::attach_host 注入；
// 插件在 initialize / uninitialize 中调用宿主能力：
// 数值注册、数值写入、共享数据接收器
// ============================================
class IPluginHost {
public:
    virtual ~IPluginHost() = default;

    /// @brief 注册插件数值到模块管理器（模块名通常使用插件名称）
    /// @param module_name 模块名称（用于规则引用与 UI 显示）
    /// @param values 数值列表（ID 需唯一，重复 ID 不覆盖已有数值）
    /// @param channels 挂载的通道列表（"A"/"B"）
    /// @return 成功返回 true
    virtual bool register_module_values(const std::string& module_name,
        const std::vector<ModuleValue>& values,
        const std::vector<std::string>& channels) = 0;

    /// @brief 注销插件注册的模块（卸载插件时调用）
    /// @param module_name 模块名称
    virtual void unregister_module(const std::string& module_name) = 0;

    /// @brief 写入数值（变化时触发规则计算与界面刷新）
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @param value 最新数值
    virtual void set_value(const std::string& module_name, const std::string& value_id,
        int value) = 0;

    /// @brief 获取宿主共享的数据接收器（插件自行配置监听端口并注册处理器）
    /// @return 数据接收器指针（宿主管理生命周期，插件不得销毁）
    virtual DataListener* data_listener() = 0;
};
