/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "IOutputDevice.h"

#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QHash>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyService>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <memory>

class QLowEnergyController;
class QBluetoothDeviceDiscoveryAgent;
class OpossumBleController;
class PawPrintsBleController;
class QTimer;

// CoyoteBleController - 郊狼 V3 蓝牙直连（QML 上下文属性 ble）
// GATT 服务与 B0 / BF / B1 指令格式见 include/README.md 的「关键协议与约定」。
// 实现 IOutputDevice：上层只经接口下发，GATT 细节留在本类内部。
// 兼任蓝牙扫描与连接入口：扫描列表里的负鼠（47L127）型号转交 OpossumBleController 写入，
// 界面上的连接状态、电量与强度读的是当前生效链路（郊狼优先，其次负鼠）。
class CoyoteBleController : public IOutputDevice {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ link_connected NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ link_device_name NOTIFY stateChanged)
    Q_PROPERTY(int battery READ link_battery NOTIFY batteryChanged)
    Q_PROPERTY(int strengthA READ link_strength_a NOTIFY strengthChanged)
    Q_PROPERTY(int strengthB READ link_strength_b NOTIFY strengthChanged)
    Q_PROPERTY(bool screenSyncAvailable READ screen_sync_available NOTIFY stateChanged)
    Q_PROPERTY(bool pawConnected READ paw_connected NOTIFY stateChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString status READ link_status NOTIFY statusChanged)
    Q_PROPERTY(QString deviceAddress READ device_address NOTIFY stateChanged)
    Q_PROPERTY(QString deviceModel READ device_model NOTIFY stateChanged)

public:
    /// @brief 蓝牙名前缀识别结果（47L1xxxx 的型号段）
    struct DeviceProfile {
        QString model;          ///< 型号段（如 47L127）
        QString label;          ///< 型号显示名（如 负鼠）
        bool supported = false; ///< 本版是否可连接（驱动已实现，含实验性驱动）
        bool output = true;     ///< 是否具备输出能力（纯输入设备不参与默认设备评选）
        QString type = QStringLiteral("unknown"); ///< 类型标识（默认设备评选用）
        SupportState state = SupportState::Unsupported; ///< 驱动支持程度（三态）
        QString hint; ///< 型号特定的行内提示（空表示按三态生成）
    };

    explicit CoyoteBleController(QObject* parent = nullptr);
    ~CoyoteBleController() override;

    void initialize();

    /// @brief 注入负鼠驱动：扫描列表里的负鼠型号连接与界面状态转交本对象
    /// @param opossum 负鼠驱动实例，可为空
    void attach_opossum(OpossumBleController* opossum);

    /// @brief 注入爪印驱动：扫描列表里的爪印型号连接与界面状态转交本对象
    /// @param pawprints 爪印驱动实例，可为空
    void attach_pawprints(PawPrintsBleController* pawprints);

    /// @brief 按蓝牙名前缀识别设备型号
    /// @param name 蓝牙名
    /// @return 识别结果；非 47L1 前缀返回未限定型号的可连接结果
    static DeviceProfile identify(const QString& name);

    bool scanning() const { return scanning_; }
    bool connected() const override { return connected_; }
    QString device_name() const { return device_name_; }
    int battery() const override { return battery_; }
    int strength_a() const override { return strength_a_; }
    int strength_b() const override { return strength_b_; }
    QVariantList devices() const { return devices_; }
    QString status() const { return status_; }
    // 界面口径的链路状态：郊狼在线取郊狼，否则取负鼠（无负鼠时与郊狼自身一致）
    bool link_connected() const;
    QString link_device_name() const;
    int link_battery() const;
    int link_strength_a() const;
    int link_strength_b() const;
    QString link_status() const;
    // 屏幕强度同步（B2）是否可用：负鼠离线时视为可用
    bool screen_sync_available() const;
    // 爪印（纯输入设备）是否已连接
    bool paw_connected() const;
    /// @brief 已连接设备的蓝牙 MAC（未连接时为空串）
    QString device_address() const override { return device_address_; }
    /// @brief 设备显示名（型号名，如「郊狼 V3」）
    QString device_label() const override { return device_label_; }
    QString device_model() const override { return device_model_; }
    QString device_type() const override { return device_type_; }
    bool device_output_capable() const { return device_output_capable_; }
    bool output_capable() const override { return device_output_capable_; }
    SupportState support_state() const override { return support_state_; }

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

    /// @brief 通道是否正在经蓝牙循环下发波形
    bool wave_sending(int channel) const override;
    /// @brief 当前下发到第几帧（1 起，未下发时为 0）
    int wave_frame(int channel) const override;
    /// @brief 正在下发的波形总帧数（未下发时为 0）
    int wave_frames(int channel) const override;

    // 以下为 IOutputDevice 适配，语义与上方同名 Q_INVOKABLE 一一对应
    // 下发强度（接口版：同 setStrength）
    void set_strength(int channel, int mode, int value) override { setStrength(channel, mode, value); }
    // 播放波形（接口版：同 playWave）
    void play_wave(int channel, const QStringList& frames) override { playWave(channel, frames); }
    // 停止通道波形（接口版：同 stopWave）
    void stop_wave(int channel) override { stopWave(channel); }
    // 写入软上限（接口版：频率/强度平衡参数固定 128，与连接就绪时一致）
    void apply_limits(int limit_a, int limit_b) override { applyLimits(limit_a, limit_b, 128, 128); }

signals:
    void stateChanged();
    void devicesChanged();
    void batteryChanged();
    void strengthChanged();
    void waveStateChanged();
    void statusChanged();
    void errorOccurred(const QString& message);
    /// @brief 屏幕强度同步（B2）不可用状态变化（负鼠链路）
    void screenSyncChanged();

private:
    // 负鼠是否为界面上的当前链路（郊狼未连接且负鼠已连接）
    bool opossum_active() const;
    // 爪印是否为界面上的当前链路（郊狼与负鼠都未连接且爪印已连接）
    bool pawprints_active() const;
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
    void do_start_scan();
    QByteArray build_b0();
    QByteArray frame_for_channel(const QStringList& frames, int index) const;
    void subscribe_battery();
    void read_battery();
    void apply_battery_value(const QByteArray& value); // 解析电量特征值并广播

    struct ChannelState {
        QStringList frames;
        int frame_index = 0;
        int pending_mode = 0; ///< 0 无 / 1 增加 / 2 减少 / 3 设为
        int pending_value = 0;
        bool waiting_ack = false;
        int pending_seq = 0;
    };

    /// @brief 取通道状态（channel: 1=A，其它按 B）
    const ChannelState& state_of(int channel) const;

    OpossumBleController* opossum_ = nullptr; ///< 负鼠驱动（扫描与连接入口共用本类）
    PawPrintsBleController* pawprints_ = nullptr; ///< 爪印驱动（纯输入设备，扫描与连接入口共用本类）
    QBluetoothDeviceDiscoveryAgent* discovery_ = nullptr;
    QLowEnergyController* controller_ = nullptr;
    QLowEnergyService* coyote_service_ = nullptr;
    QLowEnergyService* battery_service_ = nullptr;
    std::unique_ptr<QTimer> tick_timer_;
    std::unique_ptr<QTimer> battery_timer_;
    QBluetoothUuid write_char_uuid_;
    QBluetoothUuid notify_char_uuid_;
    QVariantList devices_;
    QString device_name_;
    QString pending_address_;
    QString device_address_; ///< 已连接设备的蓝牙 MAC（默认设备配置键）
    QString device_label_;   ///< 设备显示名（型号名）
    QString device_model_;   ///< 设备型号段（如 47L121）
    QString device_type_ = QStringLiteral("unknown"); ///< 类型标识（coyote / opossum / unknown）
    bool device_output_capable_ = true; ///< 已连接设备是否具备输出能力
    SupportState support_state_ = SupportState::Unsupported; ///< 已连接设备的驱动支持程度
    bool scanning_ = false;
    bool connected_ = false;
    bool ready_ = false;
    int battery_ = -1;
    int strength_a_ = 0;
    int strength_b_ = 0;
    int limit_a_ = 200;
    int limit_b_ = 200;
    int sequence_ = 0;
    int last_set_a_ = -1; ///< 最近一条绝对强度（A），-1 表示未下发过
    int last_set_b_ = -1; ///< 最近一条绝对强度（B），-1 表示未下发过
    bool identity_frame_logged_ = false; ///< 设备身份帧是否已记录
    bool opossum_link_shown_ = false; ///< 负鼠是否已成为界面当前链路（连接信号只在翻转时发）
    QString status_;
    ChannelState channel_a_;
    ChannelState channel_b_;
    QHash<QString, QBluetoothDeviceInfo> device_cache_;
    bool is_discovering_battery_ = false;
};
