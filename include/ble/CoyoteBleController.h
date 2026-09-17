/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyService>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <memory>

class QLowEnergyController;
class QBluetoothDeviceDiscoveryAgent;
class QTimer;

// ============================================
// CoyoteBleController - 郊狼 V3 蓝牙直连（QML 上下文属性 ble）
// ============================================
// 按官方 V3 蓝牙协议（DG-LAB-OPENSOURCE/coyote/v3/README_V3.md）直接控制脉冲主机：
//   服务 0x180C：写 0x150A、通知 0x150B；电量服务 0x180A：读/通知 0x1500
//   B0：每 100ms 写入 序列号+强度解读方式+双通道强度+双通道各 4 组频率/强度
//   BF：软上限 + 频率/强度平衡参数（重连后必须重写）
//   B1：主机回传 序列号 + 双通道实际强度
class CoyoteBleController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ device_name NOTIFY stateChanged)
    Q_PROPERTY(int battery READ battery NOTIFY batteryChanged)
    Q_PROPERTY(int strengthA READ strength_a NOTIFY strengthChanged)
    Q_PROPERTY(int strengthB READ strength_b NOTIFY strengthChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit CoyoteBleController(QObject* parent = nullptr);
    ~CoyoteBleController() override;

    void initialize();

    bool scanning() const { return scanning_; }
    bool connected() const { return connected_; }
    QString device_name() const { return device_name_; }
    int battery() const { return battery_; }
    int strength_a() const { return strength_a_; }
    int strength_b() const { return strength_b_; }
    QVariantList devices() const { return devices_; }
    QString status() const { return status_; }

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void connectDevice(const QString& address);
    Q_INVOKABLE void disconnectDevice();

    /// @brief 强度指令（channel: 1=A / 2=B；mode: 0 减 / 1 增 / 2 设为）
    Q_INVOKABLE void setStrength(int channel, int mode, int value);
    /// @brief 播放波形（V3 八字节帧，100ms/帧，循环播放直到 stopWave 或替换）
    Q_INVOKABLE void playWave(int channel, const QStringList& frames);
    /// @brief 停止通道波形（输出静音帧）
    Q_INVOKABLE void stopWave(int channel);
    /// @brief 写入软上限与平衡参数（BF）
    Q_INVOKABLE void applyLimits(int limitA, int limitB, int freqBalance, int strengthBalance);

signals:
    void stateChanged();
    void devicesChanged();
    void batteryChanged();
    void strengthChanged();
    void statusChanged();
    void errorOccurred(const QString& message);

private:
    void on_device_discovered(const QBluetoothDeviceInfo& info);
    void on_scan_finished();
    void on_controller_connected();
    void on_controller_disconnected();
    void on_service_state_changed(QLowEnergyService::ServiceState state);
    void on_characteristic_changed(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void write_b0();
    void write_bf();
    void ensure_characteristics();
    void set_status(const QString& text);
    QByteArray build_b0();
    QByteArray frame_for_channel(const QStringList& frames, int index) const;

    struct ChannelState {
        QStringList frames;
        int frame_index = 0;
        int pending_mode = 0; ///< 0 无 / 1 增加 / 2 减少 / 3 设为
        int pending_value = 0;
        bool waiting_ack = false;
        int pending_seq = 0;
    };

    QBluetoothDeviceDiscoveryAgent* discovery_ = nullptr;
    QLowEnergyController* controller_ = nullptr;
    QLowEnergyService* coyote_service_ = nullptr;
    QLowEnergyService* battery_service_ = nullptr;
    std::unique_ptr<QTimer> tick_timer_;
    QBluetoothUuid write_char_uuid_;
    QBluetoothUuid notify_char_uuid_;
    QVariantList devices_;
    QString device_name_;
    QString pending_address_;
    bool scanning_ = false;
    bool connected_ = false;
    bool ready_ = false;
    int battery_ = -1;
    int strength_a_ = 0;
    int strength_b_ = 0;
    int limit_a_ = 200;
    int limit_b_ = 200;
    int sequence_ = 0;
    QString status_;
    ChannelState channel_a_;
    ChannelState channel_b_;
};
