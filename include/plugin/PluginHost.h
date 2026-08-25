/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "IPlugin.h"
#include "ModuleValue.h"

#include <QJsonObject>

#include <functional>
#include <string>
#include <vector>

// ============================================
// IPluginHost - 宿主上下文接口
// 宿主在加载插件后通过 IPlugin::attach_host 注入；
// 插件在 initialize / uninitialize 中调用宿主能力：
// 数值注册/写入、数据接收（监听与分发）、配置读写、基础周期与通知
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

    // -------------------- 数据接收（解耦插件与宿主 DataListener 实现） --------------------
    /// @brief 在宿主共享监听器上开始监听端口（HTTP 协议）
    /// @param port 监听端口
    /// @return 成功返回 true（端口被占用返回 false）
    virtual bool listen_data(int port) { return false; }

    /// @brief 停止共享监听器
    virtual void stop_listening_data() {}

    /// @brief 注册数据处理器（收到匹配来源+类型的数据时调用回调）
    /// @param source 来源标识（模块名）
    /// @param type 信息类型
    /// @param handler 数据处理回调
    /// @return 成功返回 true
    virtual bool register_data_handler(const std::string& source, const std::string& type,
        const std::function<void(const QJsonObject&)>& handler) {
        (void)source;
        (void)type;
        (void)handler;
        return false;
    }

    /// @brief 注销数据处理器
    /// @param source 来源标识
    /// @param type 信息类型
    virtual void unregister_data_handler(const std::string& source, const std::string& type) {
        (void)source;
        (void)type;
    }

    // -------------------- 配置访问 --------------------
    /// @brief 读取宿主配置值（多配置合并后的最终值）
    /// @param key 点分隔配置路径（如 "python.path"）
    /// @param default_value 默认值
    /// @return 配置值
    virtual std::string get_config_value(const std::string& key,
        const std::string& default_value) {
        return default_value;
    }

    /// @brief 写入宿主配置值（写入 user 配置，持久化）
    /// @param key 点分隔配置路径
    /// @param value 配置值
    virtual void set_config_value(const std::string& key, const std::string& value) {
        (void)key;
        (void)value;
    }

    // -------------------- 基础信息与通知 --------------------
    /// @brief 获取当前调度基准周期（毫秒，供 throttle 等计算）
    /// @return 基准周期毫秒数
    virtual int base_period_ms() { return 1000; }

    /// @brief 向宿主发送用户可见通知（宿主显示弹窗/提示）
    /// @param title 标题
    /// @param message 内容
    virtual void notify(const std::string& title, const std::string& message) {
        (void)title;
        (void)message;
    }
};
