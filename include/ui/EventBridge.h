/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class EventRuntime;

// EventBridge - 事件页桥接（QML 上下文属性 eventBridge）
// 职责：汇总各模块的事件声明与用户映射、通道上限/安全上限、触发流水，
//      并转发写入、重载与「设为」互斥清理操作。
class EventBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList events READ events NOTIFY dataChanged)
    Q_PROPERTY(QVariantList triggers READ triggers NOTIFY triggersChanged)
    Q_PROPERTY(int channelA READ channel_a NOTIFY channelsChanged)
    Q_PROPERTY(int channelB READ channel_b NOTIFY channelsChanged)

public:
    /// @brief 构造
    /// @param runtime 事件运行时（不接管所有权，可为空）
    /// @param parent 父对象
    explicit EventBridge(EventRuntime* runtime, QObject* parent = nullptr);

    /// @brief 接入事件运行时与模块管理器信号
    void initialize();

    /// @brief 各模块的事件声明与当前映射
    QVariantList events() const;
    /// @brief 最近 50 条触发流水
    QVariantList triggers() const;
    /// @brief A 通道当前强度
    int channel_a() const;
    /// @brief B 通道当前强度
    int channel_b() const;

    /// @brief 重载事件配置
    Q_INVOKABLE void reload();
    /// @brief 写入通道上限与安全上限
    /// @param module 模块标识
    /// @param channel 通道（"A"/"B"）
    /// @param limit 通道上限
    /// @param safety 通道安全上限
    Q_INVOKABLE void setChannelConfig(const QString& module, const QString& channel, int limit, int safety);
    /// @brief 写入事件映射，已存的子选项映射原样保留
    /// @param module 模块标识
    /// @param event_id 事件标识
    /// @param enabled 事件启用
    /// @param wave 事件附加波形
    /// @param merge 波形合并规则文本
    /// @param weight 合并权重
    /// @param frames 激活窗口帧数
    Q_INVOKABLE void setEventConfig(const QString& module, const QString& event_id, bool enabled,
        const QString& wave, const QString& merge, double weight, int frames);
    /// @brief 写入单个子选项的输出映射
    /// @param module 模块标识
    /// @param event_id 事件标识
    /// @param option_id 子选项标识
    /// @param channel 目标通道（"A"/"B"）
    /// @param operation 操作文本
    /// @param mode 数值模式文本
    /// @param value 绝对强度
    /// @param percent 百分比
    /// @param step 步进
    Q_INVOKABLE void setOptionOutput(const QString& module, const QString& event_id, const QString& option_id,
        const QString& channel, const QString& operation, const QString& mode, int value, int percent, int step);
    /// @brief 清理指定通道上的「设为」互斥
    /// @param module 模块标识
    /// @param channel 通道（"A"/"B"）
    /// @return 被清理项的展示标识列表
    Q_INVOKABLE QVariantList resolveConflicts(const QString& module, const QString& channel);

signals:
    void dataChanged();
    void triggersChanged();
    void channelsChanged();

private:
    EventRuntime* runtime_ = nullptr; ///< 事件运行时（不持有）

    /// @brief 重载运行时并刷新界面（无运行时时直接通知）
    void apply_change();
};
