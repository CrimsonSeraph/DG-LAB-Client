/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DeviceController.h"

#include "AppConfig.h"
#include "DebugLog.h"
#include "DglabRelayServer.h"
#include "QrImageProvider.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QNetworkInterface>

#include <algorithm>

namespace {

    /// @brief 选择一个可用于手机扫码的局域网 IPv4 地址（无则回退环回）
    QString detect_lan_ip() {
        const auto interfaces = QNetworkInterface::allInterfaces();
        for (const auto& interface : interfaces) {
            const auto flags = interface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
                || flags.testFlag(QNetworkInterface::IsLoopBack)) {
                continue;
            }
            for (const auto& entry : interface.addressEntries()) {
                const QHostAddress address = entry.ip();
                if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
                    return address.toString();
                }
            }
        }
        return QStringLiteral("127.0.0.1");
    }

} // namespace

DeviceController::DeviceController(QObject* parent)
    : QObject(parent) {
}

DeviceController::~DeviceController() = default;

void DeviceController::initialize() {
    qr_provider_ = new QrImageProvider();
    relay_ = new DglabRelayServer(this);

    connect(relay_, &DglabRelayServer::statusMessage, this, &DeviceController::statusMessage);
    connect(relay_, &DglabRelayServer::errorOccurred, this, &DeviceController::errorOccurred);
    connect(relay_, &DglabRelayServer::strengthFeedback, this, &DeviceController::strengthFeedback);
    connect(relay_, &DglabRelayServer::deviceFeedback, this, &DeviceController::deviceFeedback);
    connect(relay_, &DglabRelayServer::pairingChanged, this, [this]() {
        update_pairing_url();
        emit pairingChanged();
    });

    const auto& config = AppConfig::instance();
    ip_ = QString::fromStdString(config.get_value<std::string>("app.websocket.ip", "127.0.0.1"));
    port_ = config.get_value<int>("app.websocket.port", 9999);
    if (ip_ == QStringLiteral("127.0.0.1") || ip_.isEmpty()) {
        ip_ = detect_lan_ip();
    }
    emit endpointChanged();
    LOG_MODULE("DeviceController", "initialize", LOG_INFO,
        "设备控制已就绪，配对地址: " << ip_.toStdString() << ":" << port_);

    // 内置中转服务随应用启动，配置页可立即展示配对二维码
    connectDevice();
}

QQuickImageProvider* DeviceController::qr_image_provider() const {
    return qr_provider_;
}

QString DeviceController::qr_image_url() const {
    return QStringLiteral("image://dglabqr/qr?rev=%1").arg(qr_revision_);
}

bool DeviceController::v3_paired() const {
    return relay_ != nullptr && relay_->v3_paired();
}

bool DeviceController::v4_attached() const {
    return relay_ != nullptr && relay_->v4_attached();
}

QVariantList DeviceController::v4_devices() const {
    return relay_ == nullptr ? QVariantList() : relay_->v4_devices();
}

void DeviceController::setEndpoint(const QString& ip, int port) {
    if (ip.isEmpty() || port <= 0 || port > 65535) {
        emit errorOccurred(QStringLiteral("地址或端口无效"));
        return;
    }
    ip_ = ip;
    port_ = port;
    auto& config = AppConfig::instance();
    config.set_value<std::string>("app.websocket.ip", ip.toStdString());
    config.set_value<int>("app.websocket.port", port);
    config.save_all();
    emit endpointChanged();
    update_pairing_url();
}

void DeviceController::connectDevice() {
    if (relay_ == nullptr || connected_) {
        return;
    }
    connecting_ = true;
    emit connectingChanged();

    const bool started = relay_->start();
    connecting_ = false;
    emit connectingChanged();

    connected_ = started;
    emit connectedChanged();
    if (started) {
        update_pairing_url();
        emit statusMessage(QStringLiteral("内置中转服务已启动，请用 APP 扫码配对"));
    }
}

void DeviceController::disconnectDevice() {
    if (relay_ == nullptr) {
        return;
    }
    relay_->stop();
    connected_ = false;
    pairing_url_.clear();
    ++qr_revision_;
    if (qr_provider_ != nullptr) {
        qr_provider_->set_text(QString());
    }
    emit connectedChanged();
    emit qrUrlChanged();
    emit pairingChanged();
}

void DeviceController::update_pairing_url() {
    if (relay_ == nullptr) {
        return;
    }
    pairing_url_ = relay_->pairing_url(ip_);
    ++qr_revision_;
    if (qr_provider_ != nullptr) {
        qr_provider_->set_text(pairing_url_);
    }
    emit qrUrlChanged();
}

void DeviceController::sendStrength(int channel, int mode, int value) {
    if (relay_ != nullptr) {
        relay_->send_strength(channel, mode, value);
    }
}

void DeviceController::sendWave(int channel, const QStringList& pulses, int seconds) {
    if (relay_ != nullptr) {
        relay_->send_pulse(channel, pulses, seconds);
    }
}

void DeviceController::clearQueue(int channel) {
    if (relay_ != nullptr) {
        relay_->send_clear(channel);
    }
}

void DeviceController::send_command(const QJsonObject& cmd) {
    if (!connected_) {
        emit statusMessage(QStringLiteral("中转服务未启动，命令未发送"));
        return;
    }
    const QString type = cmd.value("cmd").toString();
    if (type == QStringLiteral("send_strength")) {
        sendStrength(cmd.value("channel").toInt(1), cmd.value("mode").toInt(0), cmd.value("value").toInt(0));
    }
    else if (type == QStringLiteral("send_pulse")) {
        QStringList pulses;
        for (const auto& item : cmd.value("pulses").toArray()) {
            pulses.append(item.toString());
        }
        sendWave(cmd.value("channel").toString() == QStringLiteral("B") ? 2 : 1,
            pulses, cmd.value("duration").toInt(5));
    }
    else if (type == QStringLiteral("clear_queue")) {
        clearQueue(cmd.value("channel").toInt(1));
    }
    else if (type == QStringLiteral("set_strength")) {
        sendStrength(cmd.value("channel").toString() == QStringLiteral("B") ? 2 : 1,
            cmd.value("mode").toInt(2), cmd.value("value").toInt(0));
    }
    else {
        LOG_MODULE("DeviceController", "send_command", LOG_WARN,
            "未知命令: " << type.toStdString());
    }
}
