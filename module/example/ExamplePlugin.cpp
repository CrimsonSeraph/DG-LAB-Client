/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ExamplePlugin.h"

#include "PluginHost.h"

#include <vector>

// ============================================
// 生命周期实现（public）
// ============================================

PluginError ExamplePlugin::initialize() {
    // 通过宿主上下文注册示例数值（模块名使用插件名称，挂载到 A/B 通道）
    if (host_) {
        std::vector<ModuleValue> values;
        values.emplace_back("example_value", "示例数值", QueryPeriod::SECOND, "example_field");
        host_->register_module_values(name(), values, {"A", "B"});
    }
    // 通过日志回调上报（宿主注入回调后统一记录），类名为插件名称、方法名为 initialize
    PLUGIN_LOG(this, PluginLogLevel::Info, "示例插件初始化完成，版本: " << version());
    return PluginError::Ok;
}

void ExamplePlugin::uninitialize() {
    // 注销插件注册的模块（与 initialize 对称）
    if (host_) {
        host_->unregister_module(name());
    }
    PLUGIN_LOG(this, PluginLogLevel::Info, "示例插件反初始化完成");
}

bool ExamplePlugin::can_unload() const {
    // 空壳插件无活动资源，始终允许卸载
    return true;
}

// ============================================
// 动态库导出（extern "C" 工厂函数）
// ============================================
extern "C" {
PLUGIN_EXPORT int get_plugin_api_version() {
    // 宿主加载时校验：返回值必须等于 PLUGIN_API_VERSION
    return PLUGIN_API_VERSION;
}

PLUGIN_EXPORT IPlugin* create_plugin() {
    // 内存隔离：实例在插件内部 new，由 destroy_plugin 在插件内部 delete，
    // 宿主不得直接 delete 或跨模块分配内存
    return new ExamplePlugin();
}

PLUGIN_EXPORT void destroy_plugin(IPlugin* plugin) {
    delete plugin;
}
}
