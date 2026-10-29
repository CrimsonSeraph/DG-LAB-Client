/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "PawFrames.h"

#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyService>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

class QLowEnergyController;
class QTimer;

// PawPrintsBleController - 爪印无线按钮传感器 V1.1 的输入侧蓝牙直连
// GATT 与郊狼 V3 相同（服务 0x180C / 写 0x150A / 通知 0x150B），没有 0x180A 电量服务，
// 电量由 51 消息携带；指令与消息布局见 include/ble/PawFrames.h。
// 纯输入设备：不继承 IOutputDevice，不参与输出下发与默认设备评选；
// 连接建立后立即写 50 指令（协议要求，否则设备可能断开），写入失败重试一次并记录。
// 无实机验证：驱动按实验性上线。
class PawPrintsBleController : public QObject {
    Q_OBJECT

public:
    static constexpr int kWriteRetryMs = 1500;  ///< 50 指令回执等待上限（收到 51 视为确认）
    static constexpr int kMaxWriteAttempts = 2; ///< 50 指令写入次数上限（首次 + 重试一次）

    explicit PawPrintsBleController(QObject* parent = nullptr);
    ~PawPrintsBleController() override;

    /// @brief 初始化（状态文案归零，不连接设备）
    void initialize();

    /// @brief 连接扫描到的爪印设备（由蓝牙扫描入口按 47L120 前缀转入）
    /// @param info 扫描结果（含地址与名称）
    void connect_device(const QBluetoothDeviceInfo& info);

    /// @brief 断开并清理链路状态
    void disconnect_device();

    bool connected() const { return connected_; }
    QString device_address() const { return device_address_; }
    QString device_label() const { return device_label_; }
    QString device_model() const { return device_model_; }
    QString status() const { return status_; }
    int battery() const { return battery_; }
    int indicator_color() const { return indicator_color_; }
    int write_attempts() const { return write_attempts_; }
    bool config_acknowledged() const { return config_acknowledged_; }
    /// @brief 最近一次 F1 自检上报的 XYZ 角度阈值（依次为下限/上限各 3 组，已按 /2 换算，仅展示）
    QVariantList angle_thresholds() const { return angle_thresholds_; }

signals:
    void stateChanged();
    void statusChanged();
    void batteryChanged();
    /// @brief 51 消息：指示灯颜色 / 设备类型 / 电量
    void deviceInfoReceived(int indicator_color, int device_type, int battery);
    /// @brief 5A 消息：进入触发状态
    void triggerStarted(int event_id, int param);
    /// @brief 5B 消息：取消触发状态
    void triggerCancelled(int event_id);
    /// @brief 5C 消息：触发参数变化
    void paramChanged(int event_id, int param);
    /// @brief D0 消息：按下状态 / 加速度 / XYZ 角度 / 外部电压
    void physicalDataReceived(int pressed, int accel, int angle_x, int angle_y, int angle_z, int voltage);
    /// @brief F1 消息：自检角度阈值已更新
    void angleThresholdsChanged();
    void errorOccurred(const QString& message);

private:
    void on_controller_connected();
    void on_controller_disconnected();
    void on_service_state_changed(QLowEnergyService::ServiceState state);
    void on_characteristic_changed(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void on_write_completed(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void on_service_error(int error);
    void on_write_timeout();
    void ensure_characteristics();
    void write_config();
    void handle_message(const paw_frames::Message& message);
    void set_status(const QString& text);

    QLowEnergyController* controller_ = nullptr;
    QLowEnergyService* primary_service_ = nullptr;
    std::unique_ptr<QTimer> write_timer_;
    QBluetoothUuid write_char_uuid_;
    QBluetoothUuid notify_char_uuid_;
    QLowEnergyCharacteristic write_characteristic_;
    QLowEnergyCharacteristic notify_characteristic_;
    QString device_address_;
    QString device_label_;
    QString device_model_;
    QVariantList angle_thresholds_;
    bool connected_ = false;          ///< 蓝牙链路已建立
    bool ready_ = false;              ///< 特征已就绪，可写入 50 指令
    bool config_acknowledged_ = false; ///< 50 指令是否已收到 51 回执
    bool config_warned_ = false;      ///< 50 指令未确认是否已记录日志
    int write_attempts_ = 0;          ///< 50 指令已写入次数
    int battery_ = -1;                ///< 电量百分比（-1 未知）
    int indicator_color_ = 0;         ///< 设备回传的主指示灯颜色
    QString status_;
};
