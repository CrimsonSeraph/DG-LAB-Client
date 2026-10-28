/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DeviceController.h"

#include "AppConfig.h"
#include "CoyoteBleController.h"
#include "DebugLog.h"
#include "DglabRelayServer.h"
#include "EventMapping.h"
#include "QrImageProvider.h"
#include "RuleManager.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QTimer>

#include <algorithm>

namespace {

    constexpr int MAX_STRENGTH = 200; ///< 设备强度上限
    constexpr int MODE_SET = 2;       ///< 强度模式：设为（其余为相对增减）

    // 通道名归一为索引（0=A，1=B，其它返回 -1）
    int channel_index_of(const QString& channel) {
        if (channel.compare(QStringLiteral("A"), Qt::CaseInsensitive) == 0) {
            return 0;
        }
        if (channel.compare(QStringLiteral("B"), Qt::CaseInsensitive) == 0) {
            return 1;
        }
        return -1;
    }

    // 强度命令的通道号（字符串 "A"/"B" 或数字 1/2，其它按 A 处理）
    int strength_channel(const QJsonValue& value) {
        if (value.isString()) {
            const int index = channel_index_of(value.toString());
            return index < 0 ? 1 : index + 1;
        }
        const int channel = value.toInt(1);
        return (channel == 1 || channel == 2) ? channel : 1;
    }

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
    connect(relay_, &DglabRelayServer::errorOccurred, this, &DeviceController::report_error);
    connect(relay_, &DglabRelayServer::strengthFeedback, this, &DeviceController::strengthFeedback);
    connect(relay_, &DglabRelayServer::deviceFeedback, this, &DeviceController::deviceFeedback);
    connect(relay_, &DglabRelayServer::clientCountChanged, this, &DeviceController::appClientCountChanged);
    connect(relay_, &DglabRelayServer::pairingChanged, this, [this]() {
        update_pairing_url();
        emit pairingChanged();
        emit pairingStageChanged();
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

    // 蓝牙波形停止定时器：playWave 为循环播放，到持续时长后静音
    ble_wave_timer_a_ = new QTimer(this);
    ble_wave_timer_a_->setSingleShot(true);
    connect(ble_wave_timer_a_, &QTimer::timeout, this, [this]() {
        if (ble_ != nullptr) {
            ble_->stopWave(1);
        }
    });
    ble_wave_timer_b_ = new QTimer(this);
    ble_wave_timer_b_->setSingleShot(true);
    connect(ble_wave_timer_b_, &QTimer::timeout, this, [this]() {
        if (ble_ != nullptr) {
            ble_->stopWave(2);
        }
    });

    // 规则引擎命令统一经本类下发（规则层只依赖本类接口）
    connect(&RuleManager::instance(), &RuleManager::rule_command_ready,
        this, [this](const QJsonObject& cmd) { send_command(cmd); });

    // 内置中转服务随应用启动，配置页可立即展示配对二维码
    connectDevice();
}

void DeviceController::attach_ble(CoyoteBleController* ble) {
    ble_ = ble;
    if (ble_ != nullptr) {
        // 蓝牙连接状态直接决定输出可用性，属性需要跟着广播
        connect(ble_, &CoyoteBleController::stateChanged, this, [this]() {
            emit outputReadyChanged();
        });
    }
}

bool DeviceController::output_available() const {
    return connected_ || ble_ready();
}

int DeviceController::channel_limit(const QString& channel) const {
    const EventChannelConfig config = read_channel_config(channel.toStdString());
    // 展示上限取 min(limit, safety)：与详情页写入同源，安全上限是输出强度的硬钳制
    const int limit = std::clamp(config.limit, 0, MAX_STRENGTH);
    const int safety = std::clamp(config.safety, 0, MAX_STRENGTH);
    return std::min(limit, safety);
}

QString DeviceController::qr_image_url() const {
    return QStringLiteral("image://dglabqr/qr?rev=%1").arg(qr_revision_);
}

bool DeviceController::v3_paired() const {
    return relay_ != nullptr && relay_->v3_paired();
}

int DeviceController::app_client_count() const {
    return relay_ != nullptr ? relay_->client_count() : 0;
}

bool DeviceController::v4_attached() const {
    return relay_ != nullptr && relay_->v4_attached();
}

QVariantList DeviceController::v4_devices() const {
    return relay_ == nullptr ? QVariantList() : relay_->v4_devices();
}

int DeviceController::pairing_stage() const {
    if (!connected_) {
        return 0;
    }
    return (v3_paired() || v4_attached()) ? 2 : 1;
}

QQuickImageProvider* DeviceController::qr_image_provider() const {
    return qr_provider_;
}

void DeviceController::setEndpoint(const QString& ip, int port) {
    if (ip.isEmpty() || port <= 0 || port > 65535) {
        report_error(QStringLiteral("地址或端口无效"));
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
    notify_output_state();
    if (started) {
        last_error_.clear();
        emit lastErrorChanged();
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
    pairing_url_text_.clear();
    ++qr_revision_;
    if (qr_provider_ != nullptr) {
        qr_provider_->set_text(QString());
    }
    emit connectedChanged();
    emit qrUrlChanged();
    emit pairingChanged();
    notify_output_state();
}

void DeviceController::sendStrength(int channel, int mode, int value) {
    // 相对增减无法折算为绝对强度，不参与合并，直接下发
    if (mode != MODE_SET) {
        dispatch_strength(channel, mode, value);
        return;
    }
    apply_rule_strength(channel - 1, value);
}

void DeviceController::sendWave(int channel, const QStringList& pulses, int seconds) {
    if (ble_ready()) {
        ble_->playWave(channel, pulses);
        QTimer* timer = (channel == 1) ? ble_wave_timer_a_ : ble_wave_timer_b_;
        if (timer != nullptr) {
            timer->start(std::max(seconds, 1) * 1000);
        }
        return;
    }
    if (relay_ != nullptr) {
        relay_->send_pulse(channel, pulses, seconds);
    }
}

void DeviceController::clearQueue(int channel) {
    if (ble_ready()) {
        QTimer* timer = (channel == 1) ? ble_wave_timer_a_ : ble_wave_timer_b_;
        if (timer != nullptr) {
            timer->stop();
        }
        ble_->stopWave(channel);
        return;
    }
    if (relay_ != nullptr) {
        relay_->send_clear(channel);
    }
}

void DeviceController::send_command(const QJsonObject& cmd) {
    if (!output_available()) {
        emit statusMessage(QStringLiteral("中转服务未启动且蓝牙未连接，命令未发送"));
        return;
    }
    const QString type = cmd.value("cmd").toString();
    if (type == QStringLiteral("send_strength")) {
        sendStrength(strength_channel(cmd.value("channel")), cmd.value("mode").toInt(0),
            cmd.value("value").toInt(0));
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

void DeviceController::set_event_output(const QString& channel, int value) {
    const int index = channel_index_of(channel);
    if (index < 0) {
        return;
    }
    channel_output_[index].event_value = std::max(0, value);
    update_channel_output(index);
}

// 刷新二维码载荷与界面展示文本
void DeviceController::update_pairing_url() {
    if (relay_ == nullptr) {
        return;
    }
    pairing_url_ = relay_->pairing_url(ip_);
    pairing_url_text_ = relay_->pairing_url_display(ip_);
    ++qr_revision_;
    if (qr_provider_ != nullptr) {
        qr_provider_->set_text(pairing_url_);
    }
    emit qrUrlChanged();
}

bool DeviceController::ble_ready() const {
    return ble_ != nullptr && ble_->connected();
}

bool DeviceController::output_target_ready() const {
    return ble_ready() || v3_paired() || v4_attached();
}

void DeviceController::apply_rule_strength(int index, int value) {
    if (index < 0 || index > 1) {
        return;
    }
    channel_output_[index].rule_value = std::max(0, value);
    update_channel_output(index);
}

void DeviceController::update_channel_output(int index) {
    if (index < 0 || index > 1) {
        return;
    }
    ChannelOutputState& state = channel_output_[index];
    const QString channel = (index == 0) ? QStringLiteral("A") : QStringLiteral("B");
    int value = 0;
    if (!state.take_output(channel_limit(channel), value)) {
        return;
    }
    if (!output_target_ready()) {
        // 无输出目标：不触达下发链路，连接后按当前强度重发
        state.reset_output();
        return;
    }
    LOG_MODULE("DeviceController", "update_channel_output", LOG_DEBUG,
        "通道" << channel.toStdString() << " 事件 " << state.event_value << " / 规则 " << state.rule_value
        << " → 最终 " << value);
    dispatch_strength(index + 1, MODE_SET, value);
}

void DeviceController::dispatch_strength(int channel, int mode, int value) {
    // 蓝牙直连优先：同一台设备若同时被 APP 与蓝牙驱动会重复施加指令
    if (ble_ready()) {
        ble_->setStrength(channel, mode, value);
        return;
    }
    if (relay_ != nullptr) {
        relay_->send_strength(channel, mode, value);
    }
}

void DeviceController::report_error(const QString& message) {
    last_error_ = message;
    emit lastErrorChanged();
    emit errorOccurred(message);
}

void DeviceController::notify_output_state() {
    emit outputReadyChanged();
    emit pairingStageChanged();
}
