/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventMath.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class CoyoteBleController;
class DglabRelayServer;
class IpSelector;
class QrImageProvider;
class QQuickImageProvider;
class QTimer;

// DeviceController - 设备控制桥接（QML 上下文属性 device）
// 职责：启停应用内中转服务、生成配对二维码、向已配对的 DG-LAB APP 下发
//      强度 / 波形 / 清除指令，并把 APP 回传（强度、反馈按钮）转成信号。
// 传输由 DglabRelayServer 实现（V3 优先，V4 作为补充）；郊狼 V3 蓝牙已连接时
// 优先经 CoyoteBleController 直连输出，避免同一台设备被两条链路重复驱动。
class DeviceController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)
    Q_PROPERTY(QString ip READ ip NOTIFY endpointChanged)
    Q_PROPERTY(int port READ port NOTIFY endpointChanged)
    Q_PROPERTY(QString pairingUrl READ pairing_url NOTIFY qrUrlChanged)
    Q_PROPERTY(QString pairingUrlText READ pairing_url_text NOTIFY qrUrlChanged)
    Q_PROPERTY(QString qrImageUrl READ qr_image_url NOTIFY qrUrlChanged)
    Q_PROPERTY(bool v3Paired READ v3_paired NOTIFY pairingChanged)
    Q_PROPERTY(bool v4Attached READ v4_attached NOTIFY pairingChanged)
    Q_PROPERTY(QVariantList v4Devices READ v4_devices NOTIFY pairingChanged)
    Q_PROPERTY(int appClientCount READ app_client_count NOTIFY appClientCountChanged)
    Q_PROPERTY(QString lastError READ last_error NOTIFY lastErrorChanged)
    Q_PROPERTY(bool outputReady READ output_ready NOTIFY outputReadyChanged)
    Q_PROPERTY(int pairingStage READ pairing_stage NOTIFY pairingStageChanged)
    Q_PROPERTY(QObject* ipSelector READ ip_selector CONSTANT)

public:
    explicit DeviceController(QObject* parent = nullptr);
    ~DeviceController() override;

    /// @brief 创建中转服务与二维码提供者（不自动启动监听）
    void initialize();

    /// @brief 绑定蓝牙控制器（由 main 注入；蓝牙已连接时优先走蓝牙输出）
    void attach_ble(CoyoteBleController* ble);

    bool connected() const { return connected_; }
    bool connecting() const { return connecting_; }

    /// @brief 是否存在可用输出通道（中转服务已启动或蓝牙已连接）
    bool output_available() const;
    /// @brief 通道展示上限（全局 app.channels.<通道> 的 min(limit, safety)，钳制在 0~200）
    /// @param channel 通道（"A"/"B"）
    /// @return 上限值；通道非法时返回 0
    int channel_limit(const QString& channel) const;
    /// @brief 波形是否正在经蓝牙下发
    /// @param channel 通道（"A"/"B"）
    bool wave_sending(const QString& channel) const;
    /// @brief 正在下发的帧序号（1 起，未下发时为 0）
    /// @param channel 通道（"A"/"B"）
    int wave_frame(const QString& channel) const;
    /// @brief 正在下发的波形总帧数（未下发时为 0）
    /// @param channel 通道（"A"/"B"）
    int wave_frames(const QString& channel) const;
    /// @brief 局域网地址选择器（QML 侧经 device.ipSelector 读取候选网卡与黑/白名单）
    QObject* ip_selector() const;
    QString ip() const { return ip_; }
    int port() const { return port_; }
    QString pairing_url() const { return pairing_url_; }
    QString pairing_url_text() const { return pairing_url_text_; }
    QString qr_image_url() const;
    bool v3_paired() const;
    bool v4_attached() const;
    /// @brief APP 在线客户端数（镜像中转服务上已建立的 WebSocket 连接数）
    int app_client_count() const;
    QVariantList v4_devices() const;
    /// @brief 最近一次错误文案（所有 errorOccurred 出口同步写入）
    QString last_error() const { return last_error_; }
    /// @brief 输出可用性（镜像 output_available()，供 QML 绑定）
    bool output_ready() const { return output_available(); }
    /// @brief 配对阶段（0=未启动 / 1=等待扫码 / 2=已配对）
    int pairing_stage() const;

    /// @brief 二维码图像提供者（由 main 注册到 QML 引擎）
    QQuickImageProvider* qr_image_provider() const;

    /// @brief 更新并持久化局域网地址与端口
    Q_INVOKABLE void setEndpoint(const QString& ip, int port);
    /// @brief 启动内置中转服务（界面上的"连接"）
    Q_INVOKABLE void connectDevice();
    /// @brief 停止内置中转服务（界面上的"断开"）
    Q_INVOKABLE void disconnectDevice();

    /// @brief 强度指令（channel: 1=A / 2=B；mode: 0 减 / 1 增 / 2 设为）
    /// @note mode=2 记为规则命令最近值，与事件输出取较大值后按通道上限钳制下发；其余模式直接下发
    Q_INVOKABLE void sendStrength(int channel, int mode, int value);
    /// @brief 波形指令（V3 帧列表 + 持续秒数）
    Q_INVOKABLE void sendWave(int channel, const QStringList& pulses, int seconds);
    /// @brief 清除指定通道波形队列
    Q_INVOKABLE void clearQueue(int channel);

    /// @brief 发送规则引擎产生的命令
    void send_command(const QJsonObject& cmd);

    /// @brief 事件输出入口：记录事件最近强度，与规则命令合并后按需下发
    /// @param channel 通道（"A"/"B"）
    /// @param value 事件输出强度
    void set_event_output(const QString& channel, int value);

    /// @brief 事件波形入口：本帧该通道要播放的事件波形（每帧调用一次，同时推进播放计时）
    /// @param channel 通道（"A"/"B"）
    /// @param wave_name 事件波形名，空串表示本帧无事件波形
    void apply_event_wave(const QString& channel, const QString& wave_name);

signals:
    void connectedChanged();
    void connectingChanged();
    void endpointChanged();
    void qrUrlChanged();
    void pairingChanged();
    void appClientCountChanged();
    void strengthFeedback(int aStrength, int bStrength, int aLimit, int bLimit);
    void waveOutputChanged();
    void deviceFeedback(int channel, int button);
    void errorOccurred(const QString& message);
    void statusMessage(const QString& message);
    void lastErrorChanged();
    void outputReadyChanged();
    void pairingStageChanged();

private:
    /// @brief 规则侧波形状态（事件波形播放期间只登记不下发）
    struct RuleWaveState {
        QStringList frames;    ///< 波形帧
        int seconds = 0;       ///< 播放时长（秒）
        qint64 started_ms = 0; ///< 开始播放时刻（0 表示尚未开始）
    };

    /// @brief 解析当前使用的局域网地址（手动选中优先，其次按名单自动选择，最后回退环回）
    QString detect_lan_ip() const;
    void update_pairing_url();
    /// @brief 蓝牙是否可作为输出通道
    bool ble_ready() const;
    /// @brief 是否存在可达的输出目标（蓝牙已连接、V3 已配对或 V4 已接入）
    bool output_target_ready() const;
    /// @brief 记录规则命令的绝对强度并触发通道合并
    /// @param index 通道索引（0=A，1=B）
    /// @param value 规则命令强度
    void apply_rule_strength(int index, int value);
    /// @brief 合并规则与事件强度、按通道上限钳制并按需下发
    /// @param index 通道索引（0=A，1=B）
    void update_channel_output(int index);
    /// @brief 把强度写入输出链路（蓝牙优先，否则内置中转服务）
    /// @param channel 通道（1=A，2=B）
    /// @param mode 模式（0 减 / 1 增 / 2 设为）
    /// @param value 强度
    void dispatch_strength(int channel, int mode, int value);
    /// @brief 记录错误文案并广播（所有错误出口统一经此）
    void report_error(const QString& message);
    /// @brief 输出可用性与配对阶段变化后广播派生属性
    void notify_output_state();
    /// @brief 按当前生效链路广播一次强度回执（蓝牙在线取蓝牙，否则取中转记录值）
    void emit_authoritative_strength();
    /// @brief 事件波形状态机的一帧推进（解析帧、下发、恢复）
    /// @param index 通道索引（0=A，1=B）
    /// @param wave_name 本帧要播放的事件波形名（空串表示本帧无事件波形）
    void tick_event_wave(int index, const QString& wave_name);
    /// @brief 按发送协议解析事件波形帧（缺帧返回空）
    /// @param wave_name 事件波形名
    QStringList event_wave_frames(const QString& wave_name) const;
    /// @brief 下发事件波形并进入播放状态（蓝牙优先，否则内置中转按帧数折算秒数）
    /// @param index 通道索引（0=A，1=B）
    /// @param wave_name 事件波形名
    /// @param frames 波形帧
    void start_event_wave(int index, const QString& wave_name, const QStringList& frames);
    /// @brief 结束事件波形并恢复接管前的规则波形
    /// @param index 通道索引（0=A，1=B）
    void restore_event_wave(int index);
    /// @brief 中止事件波形并清空该通道的波形状态（通道关闭 / 设备断开）
    /// @param index 通道索引（0=A，1=B）
    void abort_event_wave(int index);
    /// @brief 规则侧波形剩余时长
    /// @param index 通道索引（0=A，1=B）
    /// @return 剩余秒数；未开始（事件波形期间暂存）返回完整时长，已结束返回 0
    int rule_wave_remaining(int index) const;
    /// @brief 波形停止定时器
    /// @param index 通道索引（0=A，1=B）
    QTimer* wave_stop_timer(int index) const;
    /// @brief 取走该通道自上次调用以来设备写入的波形帧数
    /// @param index 通道索引（0=A，1=B）
    /// @return 写入帧数；无蓝牙回执时按每帧一次返回 1
    int take_event_wave_writes(int index);

    DglabRelayServer* relay_ = nullptr;
    QrImageProvider* qr_provider_ = nullptr;
    CoyoteBleController* ble_ = nullptr; ///< 蓝牙控制器（main 注入，可为空）
    QTimer* ble_wave_timer_a_ = nullptr; ///< A 通道蓝牙波形停止定时器
    QTimer* ble_wave_timer_b_ = nullptr; ///< B 通道蓝牙波形停止定时器
    IpSelector* ip_selector_ = nullptr; ///< 局域网地址选择器（黑/白名单与手动选中地址）
    QString ip_ = QStringLiteral("127.0.0.1");
    int port_ = 9999;
    QString pairing_url_; ///< 二维码载荷（官方格式，含前缀与 DGLAB-SOCKET 标签）
    QString pairing_url_text_; ///< 界面展示的友好文本（裸 V3 服务地址）
    int qr_revision_ = 0;
    bool connected_ = false;
    bool connecting_ = false;
    QString last_error_; ///< 最近一次错误文案
    int relay_strength_a_ = 0; ///< 中转回执记录的 A 通道强度（蓝牙离线时作为展示来源）
    int relay_strength_b_ = 0; ///< 中转回执记录的 B 通道强度
    int relay_limit_a_ = 0; ///< 中转回执记录的 A 通道上限
    int relay_limit_b_ = 0; ///< 中转回执记录的 B 通道上限
    bool strength_from_ble_ = false; ///< 当前强度回执来源是否为蓝牙直连
    ChannelOutputState channel_output_[2]; ///< A/B 通道输出合并状态（0=A，1=B）
    EventWaveState event_wave_[2]; ///< A/B 通道事件波形播放状态（0=A，1=B）
    RuleWaveState rule_wave_[2];   ///< A/B 通道规则侧波形状态（0=A，1=B）
    QString last_refused_wave_[2]; ///< 最近一次缺帧拒绝的波形名（0=A，1=B，避免重复提示）
    int event_wave_writes_[2] = {0, 0}; ///< 蓝牙 B0 写入计数（0=A，1=B，事件波形播完的判据）
};
