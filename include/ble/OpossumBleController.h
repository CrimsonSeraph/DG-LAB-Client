/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "IOutputDevice.h"

#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyService>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QLowEnergyController;
class QLowEnergyService;
class QTimer;

// OpossumBleController - 负鼠振动控制器 V1 蓝牙直连（实现 IOutputDevice）
// GATT 与郊狼 V3 相同（服务 0x180C / 写 0x150A / 通知 0x150B，电量 0x180A / 0x1500），
// 指令集（B0 / B3 / B2）与波形转换为纯函数，见 include/ble/OpossumFrames.h。
// 无实机验证：驱动支持程度固定为实验性。
class OpossumBleController : public IOutputDevice {
    Q_OBJECT

public:
    explicit OpossumBleController(QObject* parent = nullptr);
    ~OpossumBleController() override;

    void initialize();

    /// @brief 连接扫描到的负鼠设备（由蓝牙扫描入口按 47L127 前缀转入）
    /// @param info 扫描结果（含地址与名称）
    void connect_device(const QBluetoothDeviceInfo& info);
    void disconnect_device();

    bool connected() const override { return connected_; }
    int battery() const override { return battery_; }
    int strength_a() const override { return strength_a_; }
    int strength_b() const override { return strength_b_; }
    QString device_address() const override { return device_address_; }
    QString device_label() const override { return device_label_; }
    QString device_model() const override { return device_model_; }
    QString device_type() const override { return QStringLiteral("opossum"); }
    bool output_capable() const override { return true; }
    SupportState support_state() const override { return SupportState::Experimental; }
    /// @brief 屏幕强度同步（B2）是否可用，写失败后置 false 且不影响 B0 / B3
    bool screen_sync_available() const { return screen_sync_available_; }
    /// @brief 当前状态文案
    QString status() const { return status_; }

    bool wave_sending(int channel) const override;
    int wave_frame(int channel) const override;
    int wave_frames(int channel) const override;
    void set_strength(int channel, int mode, int value) override;
    void play_wave(int channel, const QStringList& frames) override;
    void stop_wave(int channel) override;
    void apply_limits(int limit_a, int limit_b) override;

private:
    /// @brief 单通道波形播放状态
    struct ChannelState {
        QStringList frames;  ///< 郊狼 V3 八字节帧（下发前取强度重打包）
        int frame_index = 0; ///< 下一帧下标
    };

    void on_controller_connected();
    void on_controller_disconnected();
    void on_service_state_changed(QLowEnergyService::ServiceState state);
    void on_characteristic_changed(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void on_write_completed(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void on_service_error(int error);
    void on_b2_ack_timeout();
    void ensure_characteristics();
    void write_b0();
    void write_b3(int channel_a, int channel_b);
    void write_b2();
    void fail_b2();
    void subscribe_battery();
    void read_battery();
    void apply_battery_value(const QByteArray& value);
    void set_status(const QString& text);
    /// @brief 取通道 4 字节波形数据（无帧或帧非法时返回全 0，并累计非法帧数）
    QByteArray channel_wave_bytes(int channel);
    /// @brief 取通道播放状态（channel: 1=A，其它按 B）
    const ChannelState& state_of(int channel) const;
    /// @brief 取通道波形帧表（channel: 1=A，其它按 B）
    ChannelState& mutable_state_of(int channel);
    /// @brief 强度命令的目标值（相对增减按设备回传值折算）
    int target_strength(bool is_a, int mode, int value) const;

    QLowEnergyController* controller_ = nullptr;
    QLowEnergyService* primary_service_ = nullptr;
    QLowEnergyService* battery_service_ = nullptr;
    std::unique_ptr<QTimer> tick_timer_;
    std::unique_ptr<QTimer> battery_timer_;
    std::unique_ptr<QTimer> b2_ack_timer_;
    QBluetoothUuid write_char_uuid_;
    QBluetoothUuid notify_char_uuid_;
    QString device_address_;
    QString device_label_;
    QString device_model_;
    bool connected_ = false;             ///< 蓝牙链路已建立
    bool ready_ = false;                 ///< 特征已就绪，可写入
    int battery_ = -1;                   ///< 电量百分比（-1 未知）
    int strength_a_ = 0;                 ///< 设备回传的 A 通道强度
    int strength_b_ = 0;                 ///< 设备回传的 B 通道强度
    int limit_a_ = 200;                  ///< A 通道软上限
    int limit_b_ = 200;                  ///< B 通道软上限
    int last_set_a_ = -1;                ///< 最近一次下发的 A 通道强度（重连后重发）
    int last_set_b_ = -1;                ///< 最近一次下发的 B 通道强度
    int invalid_frame_count_ = 0;        ///< 非法波形帧累计数
    bool screen_sync_available_ = true;  ///< B2 屏幕同步是否可用
    bool b2_short_form_ = false;         ///< B2 是否已回退到 20 字节形态
    bool b2_pending_ack_ = false;        ///< 是否在等 B2 写入回执
    bool screen_sync_warned_ = false;    ///< 屏幕同步不可用是否已记录日志
    bool button_data_logged_ = false;    ///< 按键数据未接入是否已记录日志
    QString status_;
    ChannelState channel_a_;
    ChannelState channel_b_;

signals:
    void stateChanged();
    void strengthChanged();
    void batteryChanged();
    void screenSyncChanged();
    void statusChanged();
};
