/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QStringList>

// IOutputDevice - 输出设备抽象接口（通道下发的唯一依赖）
// 能力按郊狼 V3 已实现的部分定义：连接状态、电量、强度下发与回显、波形下发与进度、软上限。
// GATT 服务与指令格式由具体设备实现，上层只依赖本接口。
class IOutputDevice : public QObject {
    Q_OBJECT

public:
    // 构造：只转发 QObject 父对象
    explicit IOutputDevice(QObject* parent = nullptr) : QObject(parent) {}
    ~IOutputDevice() override = default;

    // 设备是否已连接（就绪可下发）
    virtual bool connected() const = 0;
    // 设备电量百分比（-1 表示未知）
    virtual int battery() const = 0;
    // 设备回传的实际强度（A / B）
    virtual int strength_a() const = 0;
    virtual int strength_b() const = 0;
    /// @brief 通道是否正在下发波形
    /// @param channel 通道（1=A，2=B）
    virtual bool wave_sending(int channel) const = 0;
    /// @brief 当前下发到第几帧
    /// @param channel 通道（1=A，2=B）
    /// @return 帧序号（1 起，未下发时为 0）
    virtual int wave_frame(int channel) const = 0;
    /// @brief 正在下发的波形总帧数
    /// @param channel 通道（1=A，2=B）
    /// @return 总帧数（未下发时为 0）
    virtual int wave_frames(int channel) const = 0;
    /// @brief 强度指令
    /// @param channel 通道（1=A，2=B）
    /// @param mode 模式（0 减 / 1 增 / 2 设为）
    /// @param value 强度
    virtual void set_strength(int channel, int mode, int value) = 0;
    /// @brief 播放波形（循环播放直到 stop_wave 或替换）
    /// @param channel 通道（1=A，2=B）
    /// @param frames 波形帧（设备侧帧格式的 HEX 字符串）
    virtual void play_wave(int channel, const QStringList& frames) = 0;
    /// @brief 停止通道波形
    /// @param channel 通道（1=A，2=B）
    virtual void stop_wave(int channel) = 0;
    /// @brief 写入通道软上限
    /// @param limit_a A 通道软上限
    /// @param limit_b B 通道软上限
    virtual void apply_limits(int limit_a, int limit_b) = 0;

signals:
    void outputStateChanged();    ///< 连接状态变化
    void outputStrengthChanged(); ///< 强度回执变化
    void outputWaveChanged();     ///< 波形状态与写入进度变化
};
