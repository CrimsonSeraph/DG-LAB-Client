/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "PluginHost.h"

#include <QList>
#include <QString>
#include <QWebSocket>
#include <QWebSocketServer>

#include <memory>
#include <string>
#include <vector>

// BuckshotPlugin - 恶魔轮盘外部桥接插件
// 在本机端口启动 WebSocket 服务端，接收外部桥接的 JSON 事件并写入 buckshot 模块数值
class BuckshotPlugin : public IPlugin {
public:
    // 自描述
    /// @brief 获取插件名称（模块标识，供规则引用与模块查询）
    /// @return 模块标识
    std::string name() const override { return "buckshot"; }

    /// @brief 获取插件版本号
    /// @return 版本号
    std::string version() const override { return "0.1.0"; }

    /// @brief 获取插件 API 版本
    /// @return API 版本号
    int api_version() const override { return PLUGIN_API_VERSION; }

    /// @brief 获取插件能力标志（提供可查询数值）
    /// @return 能力标志
    PluginCapability capabilities() const override { return PluginCapability::ProvidesValues; }

    // 生命周期
    /// @brief 初始化：读取端口配置、启动 WebSocket 服务端并注册数值
    /// @return 成功返回 PluginError::Ok，监听失败返回 PluginError::ResourceError
    PluginError initialize() override;

    /// @brief 反初始化：关闭全部客户端、关闭服务端、注销模块（与 initialize 对称）
    void uninitialize() override;

    /// @brief 卸载前检查：反初始化后无活动连接，允许卸载
    /// @return 允许卸载返回 true
    bool can_unload() const override { return true; }

private:
    // 数值定义
    /// @brief 创建数值列表（hp / damage / shield）
    /// @return 数值列表
    static std::vector<ModuleValue> create_default_values();

    // 连接与事件
    /// @brief 接受新接入的客户端并连接其文本帧信号
    void on_new_connection();

    /// @brief 处理一条文本帧消息（未知类型忽略并记录调试日志）
    /// @param message 文本帧内容
    void on_text_message(const QString& message);

    /// @brief 断开并释放全部客户端连接
    void close_clients();

    // 成员变量
    std::unique_ptr<QWebSocketServer> server_; ///< WebSocket 服务端
    QList<QWebSocket*> clients_;               ///< 已接入的客户端
    int port_ = 8137;                          ///< 监听端口
    bool initialized_ = false;                 ///< 初始化标志
};
