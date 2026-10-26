/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ExamplePlugin.h"
#include "PluginHost.h"

#include <QString>

#include <vector>

// 生命周期实现（public）

PluginError ExamplePlugin::initialize() {
    if (!host_) {
        PLUGIN_LOG(this, PluginLogLevel::Error, "缺少宿主上下文，无法注册模块数值");
        return PluginError::NotInitialized;
    }
    // 注册数值：模块名使用 name()，挂载到 A/B 通道
    std::vector<ModuleValue> values;
    // 数值范围 0-200，用于演示最值显示与写入钳制
    values.emplace_back("example_value", "示例数值", QueryPeriod::SECOND, "example_field", 0, 200);
    host_->register_module_values(name(), values, {"A", "B"});

    // 读取宿主配置（多配置合并后的最终值，缺省值即默认行为）
    const std::string text = host_->get_config_value("app.module.example.value", "42");
    bool ok = false;
    const int initial = QString::fromStdString(text).trimmed().toInt(&ok);
    // 写入数值：变化会触发规则计算与界面刷新
    host_->set_value(name(), "example_value", ok ? initial : 42);

    // 通过日志回调上报（宿主注入回调后统一记录）
    PLUGIN_LOG(this, PluginLogLevel::Info, "模板插件初始化完成，版本: " << version());
    return PluginError::Ok;
}

void ExamplePlugin::uninitialize() {
    // 注销插件注册的模块（与 initialize 对称）
    if (host_) {
        host_->unregister_module(name());
    }
    PLUGIN_LOG(this, PluginLogLevel::Info, "模板插件反初始化完成");
}

// 动态库导出（extern "C" 工厂函数，三个符号缺一不可）
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
