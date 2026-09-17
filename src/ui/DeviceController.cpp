/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DeviceController.h"

#include "AppConfig.h"
#include "DebugLog.h"
#include "PythonSubprocessManager.h"
#include "RuleManager.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <algorithm>

DeviceController::DeviceController(QObject* parent)
    : QObject(parent) {
}

DeviceController::~DeviceController() = default;

void DeviceController::initialize() {
    python_ = new PythonSubprocessManager(this);

    connect(python_, &PythonSubprocessManager::started, this, [this](bool success, const QString& error) {
        if (!success) {
            set_connecting(false);
            emit errorOccurred(QStringLiteral("Python 服务启动失败: ") + error);
        }
    });
    connect(python_, &PythonSubprocessManager::finished, this, [this]() {
        set_connected(false);
        set_connecting(false);
        emit statusMessage(QStringLiteral("设备连接已断开"));
    });
    connect(python_, &PythonSubprocessManager::active_message_received,
        this, &DeviceController::handle_active_message);

    // 规则引擎命令统一经本类下发（规则层只依赖本类接口）
    connect(&RuleManager::instance(), &RuleManager::rule_command_ready,
        this, [this](const QJsonObject& cmd) { send_command(cmd); });

    const auto& config = AppConfig::instance();
    const QString python_path = QString::fromStdString(config.get_value<std::string>("python.path", "python"));
    std::string bridge_module = config.get_value<std::string>("python.bridge_path", "./python/Bridge.py");
    if (bridge_module.starts_with(".")) {
        bridge_module = bridge_module.substr(1);
    }
    const QString script_path = QCoreApplication::applicationDirPath() + QString::fromStdString(bridge_module);
    python_->start_process(python_path, script_path);
    LOG_MODULE("DeviceController", "initialize", LOG_INFO, "已启动 Python 服务进程");
}

void DeviceController::connectDevice() {
    if (connecting_ || connected_) {
        return;
    }
    set_connecting(true);

    const auto& config = AppConfig::instance();
    const QString ip = QString::fromStdString(config.get_value<std::string>("app.websocket.ip", "127.0.0.1"));
    const int port = config.get_value<int>("app.websocket.port", 9999);
    const QString url = QStringLiteral("ws://%1:%2").arg(ip).arg(port);
    LOG_MODULE("DeviceController", "connectDevice", LOG_INFO, "连接地址: " + url.toStdString());

    QJsonObject set_url;
    set_url["cmd"] = "set_ws_url";
    set_url["url"] = url;
    request(set_url, 5000, [this](bool ok, const QString& msg) {
        if (!ok) {
            set_connecting(false);
            emit errorOccurred(QStringLiteral("设置 WebSocket 地址失败: ") + msg);
            return;
        }
        QJsonObject connect_cmd;
        connect_cmd["cmd"] = "connect";
        request(connect_cmd, 8000, [this](bool ok2, const QString& msg2) {
            set_connecting(false);
            set_connected(ok2);
            if (ok2) {
                emit statusMessage(QStringLiteral("已连接设备服务"));
            }
            else {
                emit errorOccurred(QStringLiteral("连接失败: ") + msg2);
            }
        });
    });
}

void DeviceController::disconnectDevice() {
    QJsonObject close_cmd;
    close_cmd["cmd"] = "close";
    request(close_cmd, 5000, [this](bool ok, const QString& msg) {
        if (ok) {
            set_connected(false);
            emit statusMessage(QStringLiteral("已断开连接"));
        }
        else {
            emit errorOccurred(QStringLiteral("断开失败: ") + msg);
        }
    });
}

void DeviceController::sendStrength(int channel, int mode, int value) {
    QJsonObject cmd;
    cmd["cmd"] = "send_strength";
    cmd["channel"] = channel;
    cmd["mode"] = mode;
    cmd["value"] = value;
    send_command(cmd);
}

void DeviceController::sendWave(int channel, const QStringList& pulses, int seconds) {
    QJsonArray array;
    for (const auto& pulse : pulses) {
        array.append(pulse);
    }
    QJsonObject cmd;
    cmd["cmd"] = "send_pulse";
    cmd["channel"] = (channel == 1) ? "A" : "B";
    cmd["pulses"] = array;
    cmd["duration"] = seconds;
    send_command(cmd);
}

void DeviceController::clearQueue(int channel) {
    QJsonObject cmd;
    cmd["cmd"] = "clear_queue";
    cmd["channel"] = channel;
    send_command(cmd);
}

void DeviceController::send_command(const QJsonObject& cmd) {
    if (!connected_) {
        LOG_MODULE("DeviceController", "send_command", LOG_WARN, "未连接，命令未发送");
        emit statusMessage(QStringLiteral("未连接，命令未发送"));
        return;
    }
    request(cmd, 5000, [this](bool ok, const QString& msg) {
        if (!ok) {
            LOG_MODULE("DeviceController", "send_command", LOG_ERROR, "命令发送失败: " + msg.toStdString());
            emit errorOccurred(QStringLiteral("命令发送失败: ") + msg);
        }
    });
}

void DeviceController::request(const QJsonObject& cmd, int timeout,
    std::function<void(bool, const QString&)> callback) {
    if (python_ == nullptr) {
        if (callback) {
            callback(false, QStringLiteral("Python 服务未初始化"));
        }
        return;
    }
    python_->call(
        cmd,
        [callback = std::move(callback)](const QJsonObject& response) {
            if (!callback) {
                return;
            }
            const bool ok = response.value("status").toString() == QStringLiteral("ok");
            callback(ok, response.value("message").toString());
        },
        timeout);
}

void DeviceController::handle_active_message(const QJsonObject& message) {
    const QJsonObject data = message.value("data").toObject();
    const QString msg_type = data.value("type").toString();

    if (msg_type == QStringLiteral("msg")) {
        const QString content = data.value("message").toString();
        if (content.startsWith(QStringLiteral("strength-"))) {
            // 格式: strength-A强度+B强度+A上限+B上限
            const QStringList parts = content.mid(9).split('+');
            if (parts.size() >= 4) {
                bool ok1 = false;
                bool ok2 = false;
                bool ok3 = false;
                bool ok4 = false;
                const int a_strength = parts[0].toInt(&ok1);
                const int b_strength = parts[1].toInt(&ok2);
                const int a_limit = parts[2].toInt(&ok3);
                const int b_limit = parts[3].toInt(&ok4);
                if (ok1 && ok2 && ok3 && ok4) {
                    emit strengthFeedback(std::clamp(a_strength, 0, 200), std::clamp(b_strength, 0, 200),
                        std::clamp(a_limit, 0, 200), std::clamp(b_limit, 0, 200));
                }
                else {
                    LOG_MODULE("DeviceController", "handle_active_message", LOG_WARN,
                        "解析强度回传失败: " + content.toStdString());
                }
            }
        }
        else if (content.startsWith(QStringLiteral("feedback-"))) {
            bool ok = false;
            const int index = content.mid(9).toInt(&ok);
            if (ok) {
                emit deviceFeedback(index < 5 ? 1 : 2, index % 5);
            }
        }
    }
    else if (msg_type == QStringLiteral("break")) {
        set_connected(false);
        emit statusMessage(QStringLiteral("对端已断开"));
    }
    else if (msg_type == QStringLiteral("error")) {
        emit errorOccurred(QStringLiteral("服务端错误: ") + data.value("message").toString());
    }
    else if (msg_type == QStringLiteral("bind")) {
        LOG_MODULE("DeviceController", "handle_active_message", LOG_DEBUG, "收到绑定消息");
    }
}

void DeviceController::set_connected(bool value) {
    if (connected_ == value) {
        return;
    }
    connected_ = value;
    emit connectedChanged();
}

void DeviceController::set_connecting(bool value) {
    if (connecting_ == value) {
        return;
    }
    connecting_ = value;
    emit connectingChanged();
}
