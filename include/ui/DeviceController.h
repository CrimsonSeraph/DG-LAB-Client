/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class CoyoteBleController;
class DglabRelayServer;
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
    Q_PROPERTY(QString qrImageUrl READ qr_image_url NOTIFY qrUrlChanged)
    Q_PROPERTY(bool v3Paired READ v3_paired NOTIFY pairingChanged)
    Q_PROPERTY(bool v4Attached READ v4_attached NOTIFY pairingChanged)
    Q_PROPERTY(QVariantList v4Devices READ v4_devices NOTIFY pairingChanged)

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
    QString ip() const { return ip_; }
    int port() const { return port_; }
    QString pairing_url() const { return pairing_url_; }
    QString qr_image_url() const;
    bool v3_paired() const;
    bool v4_attached() const;
    QVariantList v4_devices() const;

    /// @brief 二维码图像提供者（由 main 注册到 QML 引擎）
    QQuickImageProvider* qr_image_provider() const;

    /// @brief 更新并持久化局域网地址与端口
    Q_INVOKABLE void setEndpoint(const QString& ip, int port);
    /// @brief 启动内置中转服务（界面上的"连接"）
    Q_INVOKABLE void connectDevice();
    /// @brief 停止内置中转服务（界面上的"断开"）
    Q_INVOKABLE void disconnectDevice();

    /// @brief 强度指令（channel: 1=A / 2=B；mode: 0 减 / 1 增 / 2 设为）
    Q_INVOKABLE void sendStrength(int channel, int mode, int value);
    /// @brief 波形指令（V3 帧列表 + 持续秒数）
    Q_INVOKABLE void sendWave(int channel, const QStringList& pulses, int seconds);
    /// @brief 清除指定通道波形队列
    Q_INVOKABLE void clearQueue(int channel);

    /// @brief 发送规则引擎产生的命令
    void send_command(const QJsonObject& cmd);

signals:
    void connectedChanged();
    void connectingChanged();
    void endpointChanged();
    void qrUrlChanged();
    void pairingChanged();
    void strengthFeedback(int aStrength, int bStrength, int aLimit, int bLimit);
    void deviceFeedback(int channel, int button);
    void errorOccurred(const QString& message);
    void statusMessage(const QString& message);

private:
    void update_pairing_url();
    /// @brief 蓝牙是否可作为输出通道
    bool ble_ready() const;

    DglabRelayServer* relay_ = nullptr;
    QrImageProvider* qr_provider_ = nullptr;
    CoyoteBleController* ble_ = nullptr; ///< 蓝牙控制器（main 注入，可为空）
    QTimer* ble_wave_timer_a_ = nullptr; ///< A 通道蓝牙波形停止定时器
    QTimer* ble_wave_timer_b_ = nullptr; ///< B 通道蓝牙波形停止定时器
    QString ip_ = QStringLiteral("127.0.0.1");
    int port_ = 9999;
    QString pairing_url_;
    int qr_revision_ = 0;
    bool connected_ = false;
    bool connecting_ = false;
};
