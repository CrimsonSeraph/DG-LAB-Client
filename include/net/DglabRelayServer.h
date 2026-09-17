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

#include <map>
#include <memory>

class QWebSocket;
class QWebSocketServer;
class QTimer;

// ============================================
// DglabRelayServer - 应用内 DG-LAB WebSocket 中转服务
// ============================================
// 取代原先「Python Bridge + Node 官方 V2 后端」的链路：应用自己托管中转服务，
// 由 DG-LAB APP 扫码接入，应用直接与 APP 配对并下发强度/波形指令。
//
//  - V3（默认 9999）：单控制方单被控方配对，消息为
//      bind / heartbeat / break / msg(strength-通道+模式+值 | pulse-通道:[...] | clear-通道)
//  - V4（默认 9998）：控制方(clientId) + 多个被控方(tid)，消息为
//      hello / client_attached / controller_attached / message(RPC: device.op 等)
//
// 详见 DG-LAB-OPENSOURCE/socket/v2/README.md 与 dglab-kit/README.md。
class DglabRelayServer : public QObject {
    Q_OBJECT

public:
    explicit DglabRelayServer(QObject* parent = nullptr);
    ~DglabRelayServer() override;

    /// @brief 启动 V3 / V4 监听（端口取自配置）
    bool start();
    void stop();
    bool running() const { return running_; }

    QString controller_id() const { return controller_id_; }
    int v3_port() const { return v3_port_; }
    int v4_port() const { return v4_port_; }
    bool v3_paired() const;
    bool v4_attached() const;
    QString v3_pairing_url(const QString& ip) const;
    QString v4_pairing_url(const QString& ip) const;
    /// @brief 当前可用协议对应的配对链接（优先 V3）
    QString pairing_url(const QString& ip) const;

    /// @brief 目标强度（mode: 0 减 / 1 增 / 2 设为；V3 语义）
    void send_strength(int channel, int mode, int value);
    /// @brief 清除通道波形队列
    void send_clear(int channel);
    /// @brief 发送波形（V3 帧列表 + 持续秒数）
    void send_pulse(int channel, const QStringList& frames, int duration_sec);

    /// @brief V4 已接入被控方的设备列表
    QVariantList v4_devices() const;

signals:
    void runningChanged();
    void pairingChanged();
    void strengthFeedback(int aStrength, int bStrength, int aLimit, int bLimit);
    void deviceFeedback(int channel, int button);
    void statusMessage(const QString& message);
    void errorOccurred(const QString& message);
    void devicesChanged();

private:
    struct V3Connection {
        QWebSocket* ws = nullptr;
        QString id;
        bool bound = false;
    };
    struct V4Device {
        QString slot_id;
        QString name;
        QString type;
    };
    struct V4Connection {
        QWebSocket* ws = nullptr;
        QString id;
        QList<V4Device> devices;
    };

    void on_v3_connection();
    void on_v3_message(QWebSocket* ws, const QString& text);
    void on_v4_connection();
    void on_v4_message(QWebSocket* ws, const QString& text);
    void handle_v3_bind(QWebSocket* ws, const QJsonObject& message);
    void handle_v3_feedback(const QJsonObject& message);
    void handle_v4_event(const QJsonObject& data);
    QWebSocket* paired_v3_socket() const;
    V4Connection* first_v4_connection();
    const V4Connection* first_v4_connection() const;
    void send_v3_msg(const QString& inner_message);
    void send_v3_pulse_frame(int channel, const QStringList& frames);
    bool send_v4_request(const QString& method, const QJsonObject& data);
    void stop_pulse(int channel);

    QWebSocketServer* v3_server_ = nullptr;
    QWebSocketServer* v4_server_ = nullptr;
    QList<V3Connection> v3_connections_;
    QList<V4Connection> v4_connections_;
    QString controller_id_;
    QString v3_target_id_;
    int v3_port_ = 9999;
    int v4_port_ = 9998;
    bool running_ = false;
    QTimer* heartbeat_timer_ = nullptr;
    std::unique_ptr<QTimer> pulse_timer_a_;
    std::unique_ptr<QTimer> pulse_timer_b_;
    QStringList pulse_frames_a_;
    QStringList pulse_frames_b_;
    int pulse_repeat_a_ = 0;
    int pulse_repeat_b_ = 0;
};
