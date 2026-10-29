/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "CivetFrames.h"

#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyService>
#include <QObject>
#include <QString>

#include <memory>

class QLowEnergyController;
class QTimer;

// CivetBleController - 灵猫边缘控制传感器 V1 的输入侧蓝牙直连
// GATT 与郊狼 V3 相同（服务 0x180C / 写 0x150A / 通知 0x150B），另有 0x180A / 0x1500 电量特征；
// 指令与消息布局见 include/ble/CivetFrames.h。纯输入设备：不继承 IOutputDevice，
// 不参与输出下发与默认设备评选；服务发现完成后立即写 50 指令开启气压上报，断连重连走同一条路径。
// 设备需连按 5 次开机键才可被搜到，蓝牙名 47L124000。
// 无实机验证：驱动按实验性上线。
class CivetBleController : public QObject {
    Q_OBJECT

public:
    static constexpr int kReportWaitMs = 1500;    ///< 50 / 66 指令的生效等待上限（50 以首帧 D0 为回执）
    static constexpr int kMaxCommandAttempts = 2; ///< 50 / 66 指令写入次数上限（首次 + 回退一次）

    explicit CivetBleController(QObject* parent = nullptr);
    ~CivetBleController() override;

    /// @brief 初始化（状态文案归零，不连接设备）
    void initialize();

    /// @brief 连接扫描到的灵猫设备（由蓝牙扫描入口按 47L124 前缀转入）
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
    /// @brief 最近一帧气压原始值（单位 0.01 kPa）
    int pressure() const { return pressure_raw_; }
    /// @brief 因长度不足被丢弃的 D0 帧数
    int dropped_reports() const { return dropped_reports_; }
    /// @brief 50 指令是否已收到气压上报（首帧 D0 视为生效）
    bool report_acknowledged() const { return report_acknowledged_; }
    /// @brief 最近一次 66 指令的形态长度（13 / 14，0 表示未发送过）
    int command66_size() const { return command66_size_; }
    /// @brief 50 指令已写入次数
    int report_attempts() const { return report_attempts_; }

    /// @brief 写 66 指令重置气压读值（不改屏幕方向），13 字节失败回退 14 字节
    Q_INVOKABLE void resetPressureZero();

signals:
    void stateChanged();
    void statusChanged();
    /// @brief D0 气压消息：气压原始值（单位 0.01 kPa）
    void pressureReceived(int pressure_raw);
    /// @brief 电量特征：0~100 的百分比
    void batteryReceived(int percent);
    void errorOccurred(const QString& message);

private:
    QLowEnergyController* controller_ = nullptr;      ///< 当前蓝牙链路
    QLowEnergyService* primary_service_ = nullptr;     ///< 0x180C 服务
    QLowEnergyService* battery_service_ = nullptr;     ///< 0x180A 电量服务
    std::unique_ptr<QTimer> write_timer_;              ///< 50 指令生效等待
    std::unique_ptr<QTimer> command66_timer_;          ///< 66 指令写入等待
    QBluetoothUuid write_char_uuid_;                   ///< 写特征 UUID（0x150A）
    QBluetoothUuid notify_char_uuid_;                  ///< 通知特征 UUID（0x150B）
    QBluetoothUuid battery_char_uuid_;                 ///< 电量特征 UUID（0x1500）
    QLowEnergyCharacteristic write_characteristic_;    ///< 写特征
    QLowEnergyCharacteristic notify_characteristic_;   ///< 通知特征
    QLowEnergyCharacteristic battery_characteristic_;  ///< 电量特征
    QString device_address_;                           ///< 已连接设备 MAC
    QString device_label_;                             ///< 设备显示名
    QString device_model_;                             ///< 设备型号段（47L124）
    bool connected_ = false;                           ///< 蓝牙链路已建立
    bool ready_ = false;                               ///< 写特征已就绪，可写 50 指令
    bool report_acknowledged_ = false;                 ///< 是否已收到首帧 D0
    bool report_warned_ = false;                       ///< 50 指令未生效是否已记录日志
    bool drop_warned_ = false;                         ///< 长度不足丢帧是否已记录日志
    bool command66_pending_ = false;                   ///< 66 指令是否在等待写入回执
    int report_attempts_ = 0;                          ///< 50 指令已写入次数
    int command66_attempts_ = 0;                       ///< 66 指令已写入次数
    int command66_size_ = 0;                           ///< 最近一次 66 指令的形态长度
    int battery_ = -1;                                 ///< 电量百分比（-1 未知）
    int pressure_raw_ = 0;                             ///< 最近一帧气压原始值（0.01 kPa）
    int dropped_reports_ = 0;                          ///< 因长度不足丢弃的 D0 帧数
    QString status_;                                   ///< 状态文案

    void on_controller_connected();
    void on_controller_disconnected();
    void on_services_discovered();
    void on_service_state_changed(QLowEnergyService::ServiceState state);
    void on_battery_service_state_changed(QLowEnergyService::ServiceState state);
    void on_characteristic_changed(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void on_battery_characteristic_changed(const QLowEnergyCharacteristic& characteristic,
        const QByteArray& value);
    void on_characteristic_read(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void on_write_completed(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void on_service_error(int error);
    void on_report_timeout();
    void on_command66_timeout();
    void ensure_characteristics();
    void write_report_command();
    void write_command66();
    void handle_report(const QByteArray& data);
    void apply_battery(const QByteArray& value);
    void set_status(const QString& text);
};
