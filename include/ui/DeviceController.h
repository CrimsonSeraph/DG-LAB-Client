/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class PythonSubprocessManager;

// ============================================
// DeviceController - 设备控制桥接（QML 上下文属性 device）
// ============================================
// 职责：连接状态、强度 / 波形 / 清除指令的下发，以及设备回传的解析。
// 说明：当前实现经 Python 子进程转发（阶段 7 将替换为应用内 WebSocket 服务），
//      上层（HomeBridge / 规则引擎）只依赖本类接口，不感知传输方式。
class DeviceController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)

public:
    explicit DeviceController(QObject* parent = nullptr);
    ~DeviceController() override;

    /// @brief 启动 Python 子进程并接入规则引擎命令
    void initialize();

    bool connected() const { return connected_; }
    bool connecting() const { return connecting_; }

    Q_INVOKABLE void connectDevice();
    Q_INVOKABLE void disconnectDevice();

    /// @brief 强度指令（channel: 1=A / 2=B；mode: 0 减 / 1 增 / 2 设为）
    Q_INVOKABLE void sendStrength(int channel, int mode, int value);
    /// @brief 波形指令（channel: 1=A / 2=B；pulses: V3 8 字节帧；seconds: 持续秒数）
    Q_INVOKABLE void sendWave(int channel, const QStringList& pulses, int seconds);
    /// @brief 清除指定通道波形队列
    Q_INVOKABLE void clearQueue(int channel);

    /// @brief 发送规则引擎产生的命令（由 rule_command_ready 触发）
    void send_command(const QJsonObject& cmd);

signals:
    void connectedChanged();
    void connectingChanged();
    /// @brief 设备回传的通道强度与上限（A/B 各一）
    void strengthFeedback(int aStrength, int bStrength, int aLimit, int bLimit);
    /// @brief APP 反馈按钮（channel: 1/2；button: 0~4）
    void deviceFeedback(int channel, int button);
    void errorOccurred(const QString& message);
    void statusMessage(const QString& message);

private:
    void request(const QJsonObject& cmd, int timeout, std::function<void(bool, const QString&)> callback);
    void handle_active_message(const QJsonObject& message);
    void set_connected(bool value);
    void set_connecting(bool value);

    PythonSubprocessManager* python_ = nullptr;
    bool connected_ = false;
    bool connecting_ = false;
};
