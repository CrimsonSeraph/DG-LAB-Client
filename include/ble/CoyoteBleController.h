/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "BleDeviceManager.h"
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
class CivetBleController;
class PawPrintsBleController;
class QTimer;

// CoyoteBleController - 蓝牙扫描入口与默认设备的只读快照（QML 上下文属性 ble）
// 扫描是唯一入口：按 MAC 去重、按「MAC 记忆 → 蓝牙名」识别型号，再交给 BleDeviceManager 建实例。
// 本类不再持有任何设备状态：连接状态、电量、强度与波形都按设备从链路取，界面读到的是
// 默认设备（用户覆盖 → 郊狼 > 负鼠 → 先连接者）的一份只读快照，多台设备不会互相覆盖。
// 输入设备（爪印 / 灵猫）本阶段仍各有一个固定实例，只保证不与多链路冲突。
class CoyoteBleController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ device_name NOTIFY stateChanged)
    Q_PROPERTY(int battery READ battery NOTIFY batteryChanged)
    Q_PROPERTY(int strengthA READ strength_a NOTIFY strengthChanged)
    Q_PROPERTY(int strengthB READ strength_b NOTIFY strengthChanged)
    Q_PROPERTY(bool screenSyncAvailable READ screen_sync_available NOTIFY stateChanged)
    Q_PROPERTY(bool pawConnected READ paw_connected NOTIFY stateChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList connectedLinks READ connected_links NOTIFY connectedLinksChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString deviceAddress READ device_address NOTIFY stateChanged)
    Q_PROPERTY(QString deviceModel READ device_model NOTIFY stateChanged)

public:
    using SupportState = IOutputDevice::SupportState;

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

    /// @brief 注入链路管理器：扫描入口向它申请驱动实例（不接管其生命周期）
    /// @param manager 链路管理器，可为空
    void attach_link_manager(BleDeviceManager* manager);

    /// @brief 注入爪印驱动：纯输入设备本阶段仍只有一个固定实例
    /// @param pawprints 爪印驱动实例，可为空
    void attach_pawprints(PawPrintsBleController* pawprints);

    /// @brief 注入灵猫驱动：纯输入设备本阶段仍只有一个固定实例
    /// @param civet 灵猫驱动实例，可为空
    void attach_civet(CivetBleController* civet);

    /// @brief 按蓝牙名前缀识别设备型号
    /// @param name 蓝牙名
    /// @return 识别结果；未登记的 47L1 前缀与非 47L1 前缀都返回不支持
    static DeviceProfile identify(const QString& name);

    /// @brief 按型号识别设备：MAC 记忆的型号优先，其次蓝牙名前缀
    /// @param name 蓝牙名
    /// @param remembered_model 该 MAC 记忆的型号标识（coyote / pawprint / civet / opossum，空串表示无记忆）
    /// @return 识别结果；记忆命中优先于名称，两者都未命中时未登记的 47L1 与非 47L1 前缀均返回不支持
    static DeviceProfile identify(const QString& name, const QString& remembered_model);

    /// @brief 按连接后通知帧的首字节反推型号
    /// @param head 通知帧首字节
    /// @return 型号标识；0xD0（爪印与灵猫歧义）与无映射的首字节返回空串
    static QString model_from_frame_head(quint8 head);

    /// @brief 读取该 MAC 记忆的型号标识（app.devices.<MAC>.model）
    /// @param mac 设备 MAC
    /// @return 型号标识；无记忆返回空串
    static QString remembered_model(const QString& mac);

    /// @brief 记忆该 MAC 的型号标识，与已存值不同才写盘
    /// @param mac 设备 MAC
    /// @param model 型号标识
    static void remember_model(const QString& mac, const QString& model);

    bool scanning() const { return scanning_; }
    /// @brief 默认设备是否在线（多台时只看评选出的那一台）
    bool connected() const;
    /// @brief 默认设备显示名（型号名优先，其次 MAC）
    QString device_name() const;
    int battery() const;
    int strength_a() const;
    int strength_b() const;
    QString device_address() const;
    QString device_model() const;
    /// @brief 界面状态文案（默认设备优先，其次已连接的输入设备，最后是扫描入口自身）
    QString status() const;
    /// @brief 屏幕强度同步（B2）是否可用（默认设备为负鼠时按驱动回执，其它情况视为可用）
    bool screen_sync_available() const;
    /// @brief 爪印（纯输入设备）是否已连接
    bool paw_connected() const;
    /// @brief 灵猫（纯输入设备）是否已连接
    bool civet_connected() const;
    QVariantList devices() const { return devices_; }
    /// @brief 当前已连接设备快照（MAC / 型号 / 电量 / 连接序号），供设备管理页与下层只读使用
    QVariantList connected_links() const;

    Q_INVOKABLE void startScan();
    /// @brief 连接指定 MAC 的设备（按型号向链路管理器申请实例；型号无驱动时拒绝并给原因）
    /// @param address 设备蓝牙 MAC
    Q_INVOKABLE void connectDevice(const QString& address);
    /// @brief 断开当前链路（默认输出设备；没有输出设备时断开输入设备）
    Q_INVOKABLE void disconnectDevice();

    /// @brief 强度指令（channel: 1=A / 2=B；mode: 0 减 / 1 增 / 2 设为）下发给默认设备
    Q_INVOKABLE void setStrength(int channel, int mode, int value);
    /// @brief 播放波形（下发给默认设备，循环播放直到 stopWave 或替换）
    Q_INVOKABLE void playWave(int channel, const QStringList& frames);
    /// @brief 停止通道波形（下发给默认设备）
    Q_INVOKABLE void stopWave(int channel);
    /// @brief 写入软上限与平衡参数（下发给默认设备）
    Q_INVOKABLE void applyLimits(int limitA, int limitB, int freqBalance, int strengthBalance);

signals:
    void stateChanged();
    void devicesChanged();
    void batteryChanged();
    void strengthChanged();
    void waveStateChanged();
    void statusChanged();
    void errorOccurred(const QString& message);
    /// @brief 屏幕强度同步（B2）不可用状态变化
    void screenSyncChanged();
    void connectedLinksChanged();

private:
    /// @brief 默认输出设备的只读快照（用户覆盖 → 郊狼 > 负鼠 → 先连接者），无在线输出设备时为空
    IOutputDevice* snapshot_output() const;
    /// @brief 该 MAC 是否已有实例在连接（输出链路或输入设备的固定实例）
    /// @param address 设备 MAC
    bool is_connected_address(const QString& address) const;
    /// @brief 刷新扫描列表里的「已连接」标记与快照派生属性
    void refresh_device_rows();
    /// @brief 接入一台新链路的信号，按设备广播快照变化
    void watch_link(IOutputDevice* device);
    void on_device_discovered(const QBluetoothDeviceInfo& info);
    void on_scan_finished();
    void set_status(const QString& text);
    void do_start_scan();

    BleDeviceManager* manager_ = nullptr; ///< 链路管理器（生命周期归 main）
    PawPrintsBleController* pawprints_ = nullptr; ///< 爪印驱动（纯输入设备，单一固定实例）
    CivetBleController* civet_ = nullptr; ///< 灵猫驱动（纯输入设备，单一固定实例）
    QBluetoothDeviceDiscoveryAgent* discovery_ = nullptr;
    QVariantList devices_;
    QHash<QString, QBluetoothDeviceInfo> device_cache_;
    QString device_name_; ///< 最近一次连接请求的设备名（状态文案用）
    QString status_;      ///< 扫描入口自身的状态文案（无默认设备时展示）
    bool scanning_ = false;
};

// CoyoteBleDevice - 郊狼 V3 蓝牙直连驱动（每台设备一个实例，按 MAC 登记在 BleDeviceManager）
// GATT 服务与 B0 / BF / B1 指令格式见 include/README.md 的「关键协议与约定」。
// 连接状态、电量、强度回执与通道波形进度都是本实例自己的状态，多台互不覆盖。
class CoyoteBleDevice : public IOutputDevice {
    Q_OBJECT

public:
    explicit CoyoteBleDevice(QObject* parent = nullptr);
    ~CoyoteBleDevice() override;

    /// @brief 连接扫描到的郊狼设备（型号段与显示名由扫描入口识别后传入）
    /// @param info 扫描结果
    /// @param model 型号段（如 47L121）
    /// @param label 型号显示名（如 郊狼 V3）
    void connect_device(const QBluetoothDeviceInfo& info, const QString& model, const QString& label);
    void disconnect_device();

    bool connected() const override { return connected_; }
    int battery() const override { return battery_; }
    int strength_a() const override { return strength_a_; }
    int strength_b() const override { return strength_b_; }
    QString device_address() const override { return device_address_; }
    QString device_label() const override { return device_label_; }
    QString device_model() const override { return device_model_; }
    QString device_type() const override { return device_type_; }
    bool output_capable() const override { return device_output_capable_; }
    SupportState support_state() const override { return support_state_; }
    /// @brief 当前状态文案
    QString status() const { return status_; }

    /// @brief 通道是否正在经蓝牙循环下发波形
    bool wave_sending(int channel) const override;
    /// @brief 当前下发到第几帧（1 起，未下发时为 0）
    int wave_frame(int channel) const override;
    /// @brief 正在下发的波形总帧数（未下发时为 0）
    int wave_frames(int channel) const override;
    /// @brief 强度指令（channel: 1=A / 2=B；mode: 0 减 / 1 增 / 2 设为）
    void set_strength(int channel, int mode, int value) override;
    /// @brief 播放波形（循环播放直到 stop_wave 或替换）
    void play_wave(int channel, const QStringList& frames) override;
    /// @brief 停止通道波形（输出静音帧）
    void stop_wave(int channel) override;
    /// @brief 写入软上限（频率/强度平衡参数固定 128，与连接就绪时一致）
    void apply_limits(int limit_a, int limit_b) override { applyLimits(limit_a, limit_b, 128, 128); }
    /// @brief 写入软上限与平衡参数（BF）
    void applyLimits(int limitA, int limitB, int freqBalance, int strengthBalance);

signals:
    void stateChanged();
    void batteryChanged();
    void strengthChanged();
    void waveStateChanged();
    void statusChanged();

private:
    /// @brief 状态文案用的设备名（型号显示名优先，其次蓝牙名）
    QString display_name() const;
    void adopt_profile(const CoyoteBleController::DeviceProfile& profile);
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
    void subscribe_battery();
    void read_battery();
    void apply_battery_value(const QByteArray& value); // 解析电量特征值并广播

    /// @brief 单通道波形与强度回执状态（每台设备各自一份）
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

    QLowEnergyController* controller_ = nullptr;
    QLowEnergyService* coyote_service_ = nullptr;
    QLowEnergyService* battery_service_ = nullptr;
    std::unique_ptr<QTimer> tick_timer_;
    std::unique_ptr<QTimer> battery_timer_;
    QBluetoothUuid write_char_uuid_;
    QBluetoothUuid notify_char_uuid_;
    QString device_name_; ///< 蓝牙名（状态文案用）
    QString device_address_; ///< 已连接设备的蓝牙 MAC（默认设备配置键）
    QString device_label_;   ///< 设备显示名（型号名）
    QString device_model_;   ///< 设备型号段（如 47L121）
    QString device_type_ = QStringLiteral("coyote"); ///< 类型标识（coyote）
    bool device_output_capable_ = true; ///< 已连接设备是否具备输出能力
    SupportState support_state_ = SupportState::Supported; ///< 已连接设备的驱动支持程度
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
    bool model_ambiguity_logged_ = false; ///< 首帧型号歧义是否已记录
    bool model_conflict_logged_ = false; ///< 首帧反推与记忆冲突是否已记录
    QString status_;
    ChannelState channel_a_;
    ChannelState channel_b_;
    bool is_discovering_battery_ = false;
};
