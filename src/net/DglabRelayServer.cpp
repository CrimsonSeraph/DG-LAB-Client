/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DglabRelayServer.h"

#include "AppConfig.h"
#include "DebugLog.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketProtocol>
#include <QtWebSockets/QWebSocketServer>

#include <algorithm>

namespace {

    QString new_id() {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    QString channel_letter(int channel) {
        return channel == 1 ? QStringLiteral("A") : QStringLiteral("B");
    }

    QString to_text(const QJsonObject& object) {
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }

} // namespace

DglabRelayServer::DglabRelayServer(QObject* parent)
    : QObject(parent) {
    controller_id_ = new_id();

    pulse_timer_a_ = std::make_unique<QTimer>(this);
    pulse_timer_a_->setInterval(1000);
    connect(pulse_timer_a_.get(), &QTimer::timeout, this, [this]() {
        if (pulse_repeat_a_ <= 0 || pulse_frames_a_.isEmpty()) {
            pulse_timer_a_->stop();
            return;
        }
        send_v3_pulse_frame(1, pulse_frames_a_);
        --pulse_repeat_a_;
        if (pulse_repeat_a_ <= 0) {
            pulse_timer_a_->stop();
        }
    });

    pulse_timer_b_ = std::make_unique<QTimer>(this);
    pulse_timer_b_->setInterval(1000);
    connect(pulse_timer_b_.get(), &QTimer::timeout, this, [this]() {
        if (pulse_repeat_b_ <= 0 || pulse_frames_b_.isEmpty()) {
            pulse_timer_b_->stop();
            return;
        }
        send_v3_pulse_frame(2, pulse_frames_b_);
        --pulse_repeat_b_;
        if (pulse_repeat_b_ <= 0) {
            pulse_timer_b_->stop();
        }
    });

    heartbeat_timer_ = new QTimer(this);
    heartbeat_timer_->setInterval(30000);
    connect(heartbeat_timer_, &QTimer::timeout, this, [this]() {
        for (const auto& connection : v3_connections_) {
            if (connection.ws == nullptr) {
                continue;
            }
            QJsonObject heartbeat;
            heartbeat["type"] = "heartbeat";
            heartbeat["clientId"] = connection.id;
            heartbeat["targetId"] = (connection.id == v3_target_id_) ? controller_id_ : QString();
            heartbeat["message"] = "200";
            connection.ws->sendTextMessage(to_text(heartbeat));
        }
    });
}

DglabRelayServer::~DglabRelayServer() {
    stop();
}

bool DglabRelayServer::start() {
    if (running_) {
        return true;
    }
    const auto& config = AppConfig::instance();
    v3_port_ = config.get_value<int>("app.websocket.port", 9999);
    v4_port_ = config.get_value<int>("app.websocket.v4_port", 9998);

    v3_server_ = new QWebSocketServer(QStringLiteral("DG-LAB V3 Relay"), QWebSocketServer::NonSecureMode, this);
    if (!v3_server_->listen(QHostAddress::Any, static_cast<quint16>(v3_port_))) {
        const QString error = QStringLiteral("V3 服务端口 %1 监听失败: %2").arg(v3_port_).arg(v3_server_->errorString());
        delete v3_server_;
        v3_server_ = nullptr;
        emit errorOccurred(error);
        return false;
    }
    connect(v3_server_, &QWebSocketServer::newConnection, this, &DglabRelayServer::on_v3_connection);

    v4_server_ = new QWebSocketServer(QStringLiteral("DG-LAB V4 Relay"), QWebSocketServer::NonSecureMode, this);
    if (!v4_server_->listen(QHostAddress::Any, static_cast<quint16>(v4_port_))) {
        LOG_MODULE("DglabRelayServer", "start", LOG_WARN,
            "V4 服务端口监听失败（V3 仍可用）: " << v4_server_->errorString().toStdString());
        delete v4_server_;
        v4_server_ = nullptr;
    }
    else {
        connect(v4_server_, &QWebSocketServer::newConnection, this, &DglabRelayServer::on_v4_connection);
    }

    running_ = true;
    heartbeat_timer_->start();
    emit runningChanged();
    emit statusMessage(QStringLiteral("已启动内置中转服务：V3 %1 / V4 %2").arg(v3_port_).arg(v4_port_));
    LOG_MODULE("DglabRelayServer", "start", LOG_INFO,
        "内置中转服务已启动，V3 端口 " << v3_port_ << "，V4 端口 " << v4_port_);
    return true;
}

void DglabRelayServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    heartbeat_timer_->stop();
    pulse_timer_a_->stop();
    pulse_timer_b_->stop();
    pulse_repeat_a_ = 0;
    pulse_repeat_b_ = 0;

    if (v3_server_ != nullptr) {
        v3_server_->close();
        delete v3_server_;
        v3_server_ = nullptr;
    }
    if (v4_server_ != nullptr) {
        v4_server_->close();
        delete v4_server_;
        v4_server_ = nullptr;
    }
    v3_connections_.clear();
    v4_connections_.clear();
    v3_target_id_.clear();
    emit runningChanged();
    emit pairingChanged();
    emit devicesChanged();
    LOG_MODULE("DglabRelayServer", "stop", LOG_INFO, "内置中转服务已停止");
}

bool DglabRelayServer::v3_paired() const {
    return paired_v3_socket() != nullptr;
}

QWebSocket* DglabRelayServer::paired_v3_socket() const {
    if (v3_target_id_.isEmpty()) {
        return nullptr;
    }
    for (const auto& connection : v3_connections_) {
        if (connection.id == v3_target_id_ && connection.ws != nullptr) {
            return connection.ws;
        }
    }
    return nullptr;
}

bool DglabRelayServer::v4_attached() const {
    const auto* connection = first_v4_connection();
    return connection != nullptr && !connection->devices.isEmpty();
}

DglabRelayServer::V4Connection* DglabRelayServer::first_v4_connection() {
    for (auto& connection : v4_connections_) {
        if (connection.ws != nullptr) {
            return &connection;
        }
    }
    return nullptr;
}

const DglabRelayServer::V4Connection* DglabRelayServer::first_v4_connection() const {
    for (const auto& connection : v4_connections_) {
        if (connection.ws != nullptr) {
            return &connection;
        }
    }
    return nullptr;
}

QString DglabRelayServer::v3_pairing_url(const QString& ip) const {
    return QStringLiteral("ws://%1:%2/%3").arg(ip).arg(v3_port_).arg(controller_id_);
}

QString DglabRelayServer::v4_pairing_url(const QString& ip) const {
    return QStringLiteral("ws://%1:%2?tid=%3").arg(ip).arg(v4_port_).arg(controller_id_);
}

QString DglabRelayServer::pairing_url(const QString& ip) const {
    return v3_pairing_url(ip);
}

void DglabRelayServer::on_v3_connection() {
    while (v3_server_ != nullptr && v3_server_->hasPendingConnections()) {
        QWebSocket* ws = v3_server_->nextPendingConnection();
        V3Connection connection;
        connection.ws = ws;
        connection.id = new_id();
        v3_connections_.append(connection);

        QJsonObject bind;
        bind["type"] = "bind";
        bind["clientId"] = connection.id;
        bind["targetId"] = QString();
        bind["message"] = "targetId";
        ws->sendTextMessage(to_text(bind));

        connect(ws, &QWebSocket::textMessageReceived, this,
            [this, ws](const QString& text) { on_v3_message(ws, text); });
        connect(ws, &QWebSocket::disconnected, this, [this, ws]() {
            QString removed_id;
            for (int i = 0; i < v3_connections_.size(); ++i) {
                if (v3_connections_.at(i).ws == ws) {
                    removed_id = v3_connections_.at(i).id;
                    v3_connections_.removeAt(i);
                    break;
                }
            }
            if (!removed_id.isEmpty() && removed_id == v3_target_id_) {
                v3_target_id_.clear();
                stop_pulse(1);
                stop_pulse(2);
                emit pairingChanged();
                emit statusMessage(QStringLiteral("APP 已断开配对"));
            }
            ws->deleteLater();
        });

        LOG_MODULE("DglabRelayServer", "on_v3_connection", LOG_INFO, "V3 新连接已接入");
        emit statusMessage(QStringLiteral("检测到 APP 接入，等待绑定…"));
    }
}

void DglabRelayServer::on_v3_message(QWebSocket* ws, const QString& text) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        LOG_MODULE("DglabRelayServer", "on_v3_message", LOG_WARN, "V3 消息非 JSON，已忽略");
        return;
    }
    const QJsonObject message = document.object();
    const QString type = message.value("type").toString();

    if (type == QStringLiteral("bind")) {
        handle_v3_bind(ws, message);
    }
    else if (type == QStringLiteral("heartbeat")) {
        // 客户端心跳无需处理
    }
    else {
        handle_v3_feedback(message);
    }
}

void DglabRelayServer::handle_v3_bind(QWebSocket* ws, const QJsonObject& message) {
    const QString client_id = message.value("clientId").toString();
    const QString target_id = message.value("targetId").toString();

    // APP 扫码后会把二维码里的控制方 ID 作为 clientId、自己的 ID 作为 targetId 发回；
    // 也兼容相反的写法。
    QString app_id;
    if (client_id == controller_id_) {
        app_id = target_id;
    }
    else if (target_id == controller_id_) {
        app_id = client_id;
    }

    QString app_connection_id;
    for (const auto& connection : v3_connections_) {
        if (connection.ws == ws) {
            app_connection_id = connection.id;
            break;
        }
    }

    // 仅允许当前连接为发起方，或目标连接就是本连接
    if (app_id.isEmpty() || (app_id != app_connection_id && target_id != app_connection_id
            && client_id != app_connection_id)) {
        QJsonObject failed;
        failed["type"] = "bind";
        failed["clientId"] = controller_id_;
        failed["targetId"] = app_id;
        failed["message"] = "401";
        ws->sendTextMessage(to_text(failed));
        LOG_MODULE("DglabRelayServer", "handle_v3_bind", LOG_WARN, "绑定目标无效");
        return;
    }

    v3_target_id_ = app_connection_id;

    QJsonObject success;
    success["type"] = "bind";
    success["clientId"] = controller_id_;
    success["targetId"] = app_connection_id;
    success["message"] = "200";
    ws->sendTextMessage(to_text(success));

    emit pairingChanged();
    emit statusMessage(QStringLiteral("APP 配对成功"));
    LOG_MODULE("DglabRelayServer", "handle_v3_bind", LOG_INFO, "V3 配对成功");
}

void DglabRelayServer::handle_v3_feedback(const QJsonObject& message) {
    const QString content = message.value("message").toString();
    if (content.startsWith(QStringLiteral("strength-"))) {
        const QStringList parts = content.mid(9).split('+');
        if (parts.size() >= 4) {
            bool ok1 = false;
            bool ok2 = false;
            bool ok3 = false;
            bool ok4 = false;
            const int a_strength = parts.at(0).toInt(&ok1);
            const int b_strength = parts.at(1).toInt(&ok2);
            const int a_limit = parts.at(2).toInt(&ok3);
            const int b_limit = parts.at(3).toInt(&ok4);
            if (ok1 && ok2 && ok3 && ok4) {
                emit strengthFeedback(std::clamp(a_strength, 0, 200), std::clamp(b_strength, 0, 200),
                    std::clamp(a_limit, 0, 200), std::clamp(b_limit, 0, 200));
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

void DglabRelayServer::on_v4_connection() {
    while (v4_server_ != nullptr && v4_server_->hasPendingConnections()) {
        QWebSocket* ws = v4_server_->nextPendingConnection();
        const QUrlQuery query(ws->requestUrl().query());
        const QString tid = query.queryItemValue(QStringLiteral("tid"));

        if (!tid.isEmpty() && tid != controller_id_) {
            QJsonObject error;
            error["type"] = "error";
            error["message"] = "controller_not_found";
            ws->sendTextMessage(to_text(error));
            ws->close(QWebSocketProtocol::CloseCode(4001), QStringLiteral("controller_not_found"));
            ws->deleteLater();
            continue;
        }

        if (tid.isEmpty()) {
            // 外部控制方接入（协议完整性支持；本应用自身不需要）
            QJsonObject hello;
            hello["type"] = "hello";
            hello["clientId"] = new_id();
            ws->sendTextMessage(to_text(hello));
            connect(ws, &QWebSocket::textMessageReceived, this, [](const QString&) {});
            continue;
        }

        V4Connection connection;
        connection.ws = ws;
        connection.id = new_id();
        v4_connections_.append(connection);

        QJsonObject hello;
        hello["type"] = "hello";
        hello["clientId"] = connection.id;
        ws->sendTextMessage(to_text(hello));

        QJsonObject attached;
        attached["type"] = "controller_attached";
        attached["clientId"] = controller_id_;
        ws->sendTextMessage(to_text(attached));

        connect(ws, &QWebSocket::textMessageReceived, this,
            [this, ws](const QString& text) { on_v4_message(ws, text); });
        connect(ws, &QWebSocket::disconnected, this, [this, ws]() {
            for (int i = 0; i < v4_connections_.size(); ++i) {
                if (v4_connections_.at(i).ws == ws) {
                    v4_connections_.removeAt(i);
                    break;
                }
            }
            emit devicesChanged();
            emit pairingChanged();
            ws->deleteLater();
        });

        LOG_MODULE("DglabRelayServer", "on_v4_connection", LOG_INFO, "V4 被控方已接入");
        emit statusMessage(QStringLiteral("V4 APP 已接入"));
    }
}

void DglabRelayServer::on_v4_message(QWebSocket* ws, const QString& text) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    const QJsonObject frame = document.object();
    const QString type = frame.value("type").toString();
    if (type == QStringLiteral("message")) {
        handle_v4_event(frame.value("data").toObject());
    }
    else if (type == QStringLiteral("ping")) {
        QJsonObject pong;
        pong["type"] = "pong";
        pong["ts"] = QDateTime::currentMSecsSinceEpoch();
        ws->sendTextMessage(to_text(pong));
    }
}

void DglabRelayServer::handle_v4_event(const QJsonObject& data) {
    const QString event = data.value("ev").toString();
    if (event != QStringLiteral("devices.snapshot") && event != QStringLiteral("devices.patch")) {
        return;
    }
    V4Connection* connection = first_v4_connection();
    if (connection == nullptr) {
        return;
    }
    const QJsonArray devices = data.value("devices").toArray();
    connection->devices.clear();
    for (const auto& item : devices) {
        const QJsonObject device = item.toObject();
        V4Device entry;
        entry.slot_id = device.value("slotId").toString();
        entry.name = device.value("name").toString();
        entry.type = device.value("type").toString();
        if (!entry.slot_id.isEmpty()) {
            connection->devices.append(entry);
        }
    }
    emit devicesChanged();
    emit pairingChanged();
    LOG_MODULE("DglabRelayServer", "handle_v4_event", LOG_INFO,
        "V4 设备列表更新，共 " << connection->devices.size() << " 个");
}

QVariantList DglabRelayServer::v4_devices() const {
    QVariantList list;
    const auto* connection = first_v4_connection();
    if (connection == nullptr) {
        return list;
    }
    for (const auto& device : connection->devices) {
        QVariantMap item;
        item.insert(QStringLiteral("slotId"), device.slot_id);
        item.insert(QStringLiteral("name"), device.name);
        item.insert(QStringLiteral("type"), device.type);
        list.append(item);
    }
    return list;
}

void DglabRelayServer::send_v3_msg(const QString& inner_message) {
    QWebSocket* ws = paired_v3_socket();
    if (ws == nullptr) {
        return;
    }
    QJsonObject message;
    message["type"] = "msg";
    message["clientId"] = controller_id_;
    message["targetId"] = v3_target_id_;
    message["message"] = inner_message;
    ws->sendTextMessage(to_text(message));
}

void DglabRelayServer::send_v3_pulse_frame(int channel, const QStringList& frames) {
    if (frames.isEmpty()) {
        return;
    }
    QJsonArray array;
    for (const auto& frame : frames) {
        array.append(frame);
    }
    const QString inner = QStringLiteral("pulse-%1:%2")
                              .arg(channel_letter(channel))
                              .arg(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
    send_v3_msg(inner);
}

void DglabRelayServer::stop_pulse(int channel) {
    if (channel == 1) {
        pulse_repeat_a_ = 0;
        pulse_timer_a_->stop();
    }
    else {
        pulse_repeat_b_ = 0;
        pulse_timer_b_->stop();
    }
}

void DglabRelayServer::send_strength(int channel, int mode, int value) {
    if (v3_paired()) {
        send_v3_msg(QStringLiteral("strength-%1+%2+%3").arg(channel).arg(mode).arg(value));
        return;
    }

    // V4：mode 0 减少 / 1 增加 -> AddIntensity；mode 2 设为 -> SetTempIntensity；value 0 -> SetIntensity
    int action = 3;
    int payload = value;
    if (mode == 0) {
        payload = -value;
    }
    else if (mode == 2) {
        if (value == 0) {
            action = 7;
            payload = 0;
        }
        else {
            action = 4;
        }
    }
    QJsonObject data;
    data["t"] = action;
    data["c"] = channel - 1;
    data["p"] = 1;
    data["v"] = payload;
    data["im"] = true;
    send_v4_request(QStringLiteral("device.op"), data);
}

void DglabRelayServer::send_clear(int channel) {
    if (v3_paired()) {
        stop_pulse(channel);
        send_v3_msg(QStringLiteral("clear-%1").arg(channel));
        return;
    }
    QJsonObject data;
    data["c"] = channel - 1;
    send_v4_request(QStringLiteral("device.op.clear"), data);
}

void DglabRelayServer::send_pulse(int channel, const QStringList& frames, int duration_sec) {
    if (frames.isEmpty()) {
        return;
    }

    if (!v3_paired()) {
        QJsonArray array;
        for (const auto& frame : frames) {
            array.append(frame);
        }
        QJsonObject data;
        data["t"] = 0;
        data["c"] = channel - 1;
        data["p"] = 1;
        data["d"] = std::max(duration_sec, 1) * 1000;
        data["v"] = array;
        data["ver"] = 3;
        data["im"] = true;
        send_v4_request(QStringLiteral("device.op"), data);
        return;
    }

    // 立即清空并按波形长度循环补发，保证持续时长内连续输出
    stop_pulse(channel);
    send_v3_msg(QStringLiteral("clear-%1").arg(channel));
    send_v3_pulse_frame(channel, frames);

    const int wave_ms = static_cast<int>(frames.size()) * 100;
    const int total_ms = std::max(duration_sec, 1) * 1000;
    const int repeats = (wave_ms > 0) ? (total_ms / wave_ms) - 1 : 0;
    if (repeats <= 0) {
        return;
    }

    if (channel == 1) {
        pulse_frames_a_ = frames;
        pulse_repeat_a_ = repeats;
        pulse_timer_a_->setInterval(std::max(200, wave_ms));
        pulse_timer_a_->start();
    }
    else {
        pulse_frames_b_ = frames;
        pulse_repeat_b_ = repeats;
        pulse_timer_b_->setInterval(std::max(200, wave_ms));
        pulse_timer_b_->start();
    }
}

bool DglabRelayServer::send_v4_request(const QString& method, const QJsonObject& data) {
    V4Connection* connection = first_v4_connection();
    if (connection == nullptr || connection->ws == nullptr) {
        return false;
    }

    QString slot_id;
    for (const auto& device : connection->devices) {
        if (device.type.startsWith(QStringLiteral("COYOTE"))) {
            slot_id = device.slot_id;
            break;
        }
    }
    if (slot_id.isEmpty() && !connection->devices.isEmpty()) {
        slot_id = connection->devices.first().slot_id;
    }
    if (slot_id.isEmpty()) {
        emit errorOccurred(QStringLiteral("V4 未发现可用设备"));
        return false;
    }

    QJsonObject request = data;
    request["s"] = slot_id;

    QJsonObject rpc;
    rpc["t"] = "req";
    rpc["reqId"] = new_id();
    rpc["m"] = method;
    rpc["data"] = request;

    QJsonObject frame;
    frame["type"] = "message";
    frame["clientId"] = connection->id;
    frame["data"] = rpc;
    connection->ws->sendTextMessage(to_text(frame));
    return true;
}
