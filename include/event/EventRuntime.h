/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventEngine.h"

#include <QObject>

#include <map>
#include <set>
#include <string>
#include <vector>

// 前置声明
class QTimer;

// EventRuntime - 事件引擎宿主（数值变化入口与固定帧推进）
// 连接模块管理器的数值变化与插件状态变化，按 100ms 帧推进引擎，
// 对外暴露通道强度与触发流水；不参与蓝牙与网络输出
class EventRuntime : public QObject {
    Q_OBJECT

public:
    /// @brief 构造（不访问 ModuleManager，帧定时器留到 initialize 创建）
    /// @param parent 父对象
    explicit EventRuntime(QObject* parent = nullptr);

    /// @brief 连接 ModuleManager、启动 100ms 帧定时器并首次载入配置
    void initialize();

    /// @brief 重新载入所有模块的事件声明与映射
    void reload();

    /// @brief 注册内置设备的事件声明（与插件侧车声明并列的第二个声明来源）
    /// @param module_name 模块标识（同时是数值模块名，如 "paw"）
    /// @param events 事件声明列表；空列表表示清除该模块的内置声明
    /// @note 同一模块标识重复注册整体替换；该模块标识已由插件声明事件时以插件声明为准，内置声明被忽略；
    ///       已初始化时立即重新载入，未初始化时在 initialize() 的首次载入生效
    void register_builtin_events(const std::string& module_name,
        const std::vector<EventDefinition>& events);

    /// @brief 注销某模块标识的内置声明并丢弃其引擎运行期状态（设备断开时调用）
    /// @param module_name 模块标识（实例模块名，如 "paw_430C"）
    /// @note 只影响该模块标识：其它内置声明与插件声明保持原样；未登记该标识时只清运行期状态
    void unregister_builtin_events(const std::string& module_name);

    /// @brief 只读访问引擎（供桥接读取流水）
    /// @return 事件引擎
    const EventEngine& engine() const;

    /// @brief 已注册的内置事件声明（只读，供桥接与测试核对）
    /// @return 模块标识 → 事件声明
    const std::map<std::string, std::vector<EventDefinition>>& builtin_events() const {
        return builtin_events_;
    }

    /// @brief 设置运行期启用集合（启用级联：依赖集合中的事件视为启用）
    /// @param events 事件标识集合，写作 "<模块标识>/<事件 ID>"
    /// @note 集合变化时立即重新载入映射，下一次帧推进即生效；不写回配置
    void set_cascade_events(const std::set<std::string>& events);

    /// @brief 当前运行期启用集合
    /// @return 事件标识集合
    const std::set<std::string>& cascade_events() const { return cascade_events_; }

    /// @brief 通道当前强度
    /// @param channel 通道（"A"/"B"）
    /// @return 强度整数，通道非法返回 0
    int channel_output(const std::string& channel) const;

    static constexpr int FRAME_INTERVAL_MS = 100; ///< 帧间隔（毫秒）

signals:
    /// @brief 每帧推进后发出通道强度
    /// @param channel_a A 通道强度
    /// @param channel_b B 通道强度
    void channelsChanged(int channel_a, int channel_b);

    /// @brief 每帧推进后发出（触发流水可能已更新）
    void triggersChanged();

    /// @brief 配置重新载入完成
    void reloaded();

private:
    EventEngine engine_; ///< 事件引擎
    QTimer* frame_timer_ = nullptr; ///< 帧定时器（initialize 前为空）
    std::set<std::string> cascade_events_; ///< 运行期启用集合（键为 "<模块标识>/<事件 ID>"）
    std::map<std::string, std::vector<EventDefinition>> builtin_events_; ///< 内置事件声明（键为模块标识）

    /// @brief 载入单个模块的事件声明与映射（声明来源无关，插件与内置共用）
    /// @param module_name 模块标识
    /// @param definitions 事件声明列表
    void configure_module(const std::string& module_name, const std::vector<EventDefinition>& definitions);

    /// @brief 帧定时器触发：推进一帧并广播通道强度与流水
    void on_frame_tick();
};
