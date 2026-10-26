/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "PluginHost.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QTimer>

#include <memory>
#include <string>
#include <vector>

// WarThunderPlugin - 战争雷霆空战遥测插件
// 周期轮询本地 127.0.0.1:8111 遥测接口，把速度、高度、姿态、动力与构型等遥测写入 warthunder 模块
class WarThunderPlugin : public IPlugin {
public:
    // 自描述
    /// @brief 获取插件名称（模块标识，供规则引用与模块查询）
    /// @return 模块标识
    std::string name() const override { return "warthunder"; }

    /// @brief 获取插件版本号
    /// @return 版本号
    std::string version() const override { return "0.2.0"; }

    /// @brief 获取插件 API 版本
    /// @return API 版本号
    int api_version() const override { return PLUGIN_API_VERSION; }

    /// @brief 获取插件能力标志（提供可查询数值）
    /// @return 能力标志
    PluginCapability capabilities() const override { return PluginCapability::ProvidesValues; }

    // 生命周期
    /// @brief 初始化：读取配置、注册数值并启动轮询
    /// @return 成功返回 PluginError::Ok
    PluginError initialize() override;

    /// @brief 反初始化：停止轮询、断开在途请求、注销模块（与 initialize 对称）
    void uninitialize() override;

    /// @brief 卸载前检查：反初始化后无活动请求，允许卸载
    /// @return 允许卸载返回 true
    bool can_unload() const override { return true; }

private:
    // 数值与配置解析
    /// @brief 创建数值列表（数值表与定标见 module/warthunder/README.md）
    /// @return 数值列表
    static std::vector<ModuleValue> create_default_values();

    /// @brief 解析轮询周期配置
    /// @param text 配置文本（毫秒）
    /// @return 周期毫秒数（非法值回退 500，下限 100）
    static int parse_period_ms(const std::string& text);

    // 遥测轮询
    /// @brief 发起一次遥测请求（在途请求未完成时跳过本次）
    void request_state();

    /// @brief 处理遥测响应：按数值表写入当帧出现的字段，失败仅限流记录调试日志
    /// @param reply 已完成的应答
    void on_state_reply(QNetworkReply* reply);

    /// @brief 限流输出调试日志（间隔内的重复内容丢弃）
    /// @param message 日志内容
    void log_throttled(const std::string& message);

    // 成员变量
    std::unique_ptr<QTimer> timer_;                  ///< 轮询定时器
    std::unique_ptr<QNetworkAccessManager> network_; ///< 遥测请求管理器
    QNetworkReply* reply_ = nullptr;                 ///< 在途请求（由 network_ 持有）
    QString endpoint_;                               ///< 遥测接口地址
    int period_ms_ = 500;                            ///< 轮询周期（毫秒）
    qint64 last_log_ms_ = 0;                         ///< 上次调试日志时间戳
    bool initialized_ = false;                       ///< 初始化标志
};
