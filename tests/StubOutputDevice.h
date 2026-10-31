/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "IOutputDevice.h"

#include <QString>
#include <QStringList>

// StubOutputDevice - 测试用合成输出设备（实现 IOutputDevice，不连接真实蓝牙）
// 只记录被下发的指令与自身状态，用于验证链路登记表与默认设备评选的纯逻辑。
class StubOutputDevice : public IOutputDevice {
    Q_OBJECT

public:
    /// @brief 构造一台合成设备
    /// @param address 设备 MAC
    /// @param type 类型标识（coyote / opossum）
    explicit StubOutputDevice(const QString& address, const QString& type, QObject* parent = nullptr)
        : IOutputDevice(parent)
        , address_(address)
        , type_(type) {
    }

    /// @brief 设置连接状态并按驱动口径广播状态变化
    /// @param connected 是否已连接
    void set_connected(bool connected) {
        if (connected_ == connected) {
            return;
        }
        connected_ = connected;
        if (!connected) {
            // 与真实驱动一致：断开后波形状态随链路失效，不影响其它设备
            frames_a_ = 0;
            frames_b_ = 0;
            frame_a_ = 0;
            frame_b_ = 0;
        }
        emit outputStateChanged();
    }

    /// @brief 设置电量并广播
    /// @param battery 电量百分比（-1 表示未知）
    void set_battery(int battery) {
        battery_ = battery;
        emit outputStateChanged();
    }

    /// @brief 模拟设备回传的强度并广播
    /// @param a A 通道强度
    /// @param b B 通道强度
    void emit_strength(int a, int b) {
        strength_a_ = a;
        strength_b_ = b;
        emit outputStrengthChanged();
    }

    /// @brief 设置型号段与显示名
    /// @param model 型号段（如 47L121）
    /// @param label 显示名（如 郊狼 V3）
    void set_identity(const QString& model, const QString& label) {
        model_ = model;
        label_ = label;
    }

    /// @brief 设置驱动支持程度
    /// @param state 支持程度
    void set_support_state(SupportState state) { support_state_ = state; }

    bool connected() const override { return connected_; }
    int battery() const override { return battery_; }
    int strength_a() const override { return strength_a_; }
    int strength_b() const override { return strength_b_; }
    QString device_address() const override { return address_; }
    QString device_label() const override { return label_; }
    QString device_model() const override { return model_; }
    QString device_type() const override { return type_; }
    bool output_capable() const override { return output_capable_; }
    SupportState support_state() const override { return support_state_; }
    bool wave_sending(int channel) const override { return (channel == 1 ? frames_a_ : frames_b_) > 0; }
    int wave_frame(int channel) const override { return (channel == 1 ? frame_a_ : frame_b_); }
    // 帧数内部按 qsizetype 记账（与 QStringList::size() 同宽），对外接口保持 IOutputDevice 的 int
    int wave_frames(int channel) const override { return static_cast<int>(channel == 1 ? frames_a_ : frames_b_); }

    /// @brief 记录强度指令（合成设备不下发）
    void set_strength(int channel, int mode, int value) override {
        last_channel_ = channel;
        last_mode_ = mode;
        last_value_ = value;
    }

    /// @brief 记录波形下发（循环帧数用于验证波形状态按设备隔离）
    void play_wave(int channel, const QStringList& frames) override {
        if (channel == 1) {
            frames_a_ = frames.size();
            frame_a_ = frames.isEmpty() ? 0 : 1;
        }
        else {
            frames_b_ = frames.size();
            frame_b_ = frames.isEmpty() ? 0 : 1;
        }
        emit outputWaveChanged();
    }

    /// @brief 停止通道波形
    void stop_wave(int channel) override {
        if (channel == 1) {
            frames_a_ = 0;
            frame_a_ = 0;
        }
        else {
            frames_b_ = 0;
            frame_b_ = 0;
        }
        emit outputWaveChanged();
    }

    /// @brief 记录软上限（合成设备不写入）
    void apply_limits(int limit_a, int limit_b) override {
        limit_a_ = limit_a;
        limit_b_ = limit_b;
    }

    /// @brief 设置是否具备输出能力（验证纯输入设备不参与评选）
    /// @param capable 是否具备输出能力
    void set_output_capable(bool capable) { output_capable_ = capable; }

    int last_channel() const { return last_channel_; } ///< 最近一次强度指令的通道
    int last_mode() const { return last_mode_; }       ///< 最近一次强度指令的模式
    int last_value() const { return last_value_; }     ///< 最近一次强度指令的值
    int limit_a() const { return limit_a_; }           ///< 最近一次软上限（A）
    int limit_b() const { return limit_b_; }           ///< 最近一次软上限（B）

private:
    QString address_; ///< 设备 MAC
    QString type_;    ///< 类型标识
    QString model_;   ///< 型号段
    QString label_;   ///< 显示名
    bool connected_ = false;
    int battery_ = -1;
    int strength_a_ = 0;
    int strength_b_ = 0;
    qsizetype frames_a_ = 0; ///< A 通道下发帧数（0 表示未下发）
    qsizetype frames_b_ = 0; ///< B 通道下发帧数
    int frame_a_ = 0;  ///< A 通道当前帧
    int frame_b_ = 0;  ///< B 通道当前帧
    int limit_a_ = 200;
    int limit_b_ = 200;
    int last_channel_ = 0;
    int last_mode_ = 0;
    int last_value_ = 0;
    bool output_capable_ = true;
    SupportState support_state_ = SupportState::Supported;
};
