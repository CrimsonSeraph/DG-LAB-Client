/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DeviceController.h"

#include "AppConfig.h"
#include "BleDeviceManager.h"
#include "DebugLog.h"
#include "DglabRelayServer.h"
#include "EventMapping.h"
#include "IOutputDevice.h"
#include "IpSelector.h"
#include "QrImageProvider.h"
#include "RuleManager.h"
#include "WaveLibrary.h"

#include <QDateTime>
#include <QJsonArray>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>

namespace {

    constexpr int MAX_STRENGTH = 200; ///< 设备强度上限
    constexpr int MODE_SET = 2;       ///< 强度模式：设为（其余为相对增减）
    constexpr const char* kTargetCoyote = "coyote"; ///< 输出目标：郊狼 V3 蓝牙直连

    // 通道输出目标配置键（app.output.<A|B>.target）
    QString output_target_key(int index) {
        return QStringLiteral("app.output.%1.target").arg(index == 0 ? QStringLiteral("A") : QStringLiteral("B"));
    }

    // 通道索引对应的通道名
    QString channel_name_of(int index) {
        return index == 0 ? QStringLiteral("A") : QStringLiteral("B");
    }

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

    // 读取 app.devices.<MAC> 设备段（MAC 大小写不敏感；设备段不存在时返回空对象）
    nlohmann::json device_section_of(const QString& mac) {
        const nlohmann::json devices = AppConfig::instance().get_value<nlohmann::json>(
            "app.devices", nlohmann::json::object());
        if (!devices.is_object()) {
            return nlohmann::json::object();
        }
        const QString device = mac.trimmed().toUpper();
        for (auto it = devices.begin(); it != devices.end(); ++it) {
            if (QString::fromStdString(it.key()).compare(device, Qt::CaseInsensitive) == 0 && it.value().is_object()) {
                return it.value();
            }
        }
        return nlohmann::json::object();
    }

    // 取设备段里的数值键（键缺失或不是数字时返回 false，value 保持原值）
    bool device_section_value(const nlohmann::json& section, const char* key, int* value) {
        const nlohmann::json::const_iterator it = section.find(key);
        if (it == section.end() || !it->is_number()) {
            return false;
        }
        *value = static_cast<int>(it->get<double>());
        return true;
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

} // namespace

DeviceController::DeviceController(QObject* parent)
    : QObject(parent)
    , ip_selector_(new IpSelector(this)) {
}

DeviceController::~DeviceController() = default;

void DeviceController::initialize() {
    qr_provider_ = new QrImageProvider();
    relay_ = new DglabRelayServer(this);

    connect(relay_, &DglabRelayServer::statusMessage, this, &DeviceController::statusMessage);
    connect(relay_, &DglabRelayServer::errorOccurred, this, &DeviceController::report_error);
    // 中转回执只记录不外发：蓝牙在线时以蓝牙回执为准，避免两条链路互相覆盖导致数值抖动
    connect(relay_, &DglabRelayServer::strengthFeedback, this,
        [this](int a_strength, int b_strength, int a_limit, int b_limit) {
            relay_strength_a_ = a_strength;
            relay_strength_b_ = b_strength;
            relay_limit_a_ = a_limit;
            relay_limit_b_ = b_limit;
            if (!ble_ready()) {
                emit strengthFeedback(a_strength, b_strength, a_limit, b_limit);
            }
        });
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
        if (IOutputDevice* device = active_output(0)) {
            device->stop_wave(1);
        }
        rule_wave_[0] = RuleWaveState();
    });
    ble_wave_timer_b_ = new QTimer(this);
    ble_wave_timer_b_->setSingleShot(true);
    connect(ble_wave_timer_b_, &QTimer::timeout, this, [this]() {
        if (IOutputDevice* device = active_output(1)) {
            device->stop_wave(2);
        }
        rule_wave_[1] = RuleWaveState();
    });

    // 配置变化（外部手改文件或应用内写入）后按新上下限广播；延到事件循环再读，
    // 避免在配置通知持锁期间回调 AppConfig 的加锁接口
    AppConfig::instance().add_config_listener("all", [self = QPointer<DeviceController>(this)]() {
        if (self.isNull()) {
            return;
        }
        QTimer::singleShot(0, self.data(), [self]() {
            if (!self.isNull()) {
                self->refresh_limits();
            }
        });
    });
    refresh_limits();

    // 规则引擎命令统一经本类下发（规则层只依赖本类接口）
    connect(&RuleManager::instance(), &RuleManager::rule_command_ready,
        this, [this](const QJsonObject& cmd) { send_command(cmd); });

    // 内置中转服务随应用启动，配置页可立即展示配对二维码
    connectDevice();

}

void DeviceController::attach_output(IOutputDevice* output) {
    if (output == nullptr || outputs_.contains(output)) {
        return;
    }
    outputs_.append(output);
    // 设备先于本类销毁时把它移出列表：设备析构期间仍会发状态信号，回调一路走到
    // online_outputs() 逐个解引用，残留裸指针会踩到已销毁对象（实测段错误）
    connect(output, &QObject::destroyed, this, [this, output]() {
        outputs_.removeAll(output);
        connection_order_.remove(output);
        if (last_default_output_ == output) {
            last_default_output_ = nullptr;
        }
    });
    // 蓝牙连接状态直接决定输出可用性，属性需要跟着广播
    connect(output, &IOutputDevice::outputStateChanged, this, [this, output]() {
        if (output->connected()) {
            // 连接顺序用于同类型多台设备时「先连接的优先」评选
            if (!connection_order_.contains(output)) {
                connection_order_.insert(output, ++connection_seq_);
            }
            notify_experimental_device(output);
        }
        // 只有默认设备换了（离线或另一台接管）才清空通道波形；其它设备断开重连不影响在用链路
        if (IOutputDevice* current = default_output(); current != last_default_output_) {
            const bool had_default = last_default_output_ != nullptr;
            last_default_output_ = current;
            abort_event_wave(0);
            abort_event_wave(1);
            for (int index = 0; index < 2; ++index) {
                channel_output_[index].reset_output();
                // 从一台在线设备换到另一台时才重发当前强度：首次连接不补发历史强度，避免意外加档
                if (had_default && current != nullptr) {
                    update_channel_output(index);
                }
            }
        }
        emit outputReadyChanged();
        // 在线设备与默认设备都随链路变化，界面列表与上下限跟着刷新
        emit outputDevicesChanged();
        emit defaultDeviceChanged();
        refresh_limits();
        // 链路切换后强度来源随之切换，需要按新链路广播一次
        emit_authoritative_strength();
    });
    // 蓝牙回执是蓝牙在线时的强度唯一来源（郊狼 B1 每 100ms 回传，负鼠 B3 按变化回传）
    connect(output, &IOutputDevice::outputStrengthChanged, this, [this, output]() {
        if (output->connected()) {
            emit_authoritative_strength();
        }
    });
    connect(output, &IOutputDevice::outputWaveChanged, this, [this]() {
        // 每次 B0 写入代表设备吃掉一帧波形，事件波形据此判断是否播完
        ++event_wave_writes_[0];
        ++event_wave_writes_[1];
        emit waveOutputChanged();
    });
}

void DeviceController::detach_output(IOutputDevice* output) {
    if (output == nullptr || !outputs_.contains(output)) {
        return;
    }
    outputs_.removeAll(output);
    connection_order_.remove(output);
    if (last_default_output_ == output) {
        last_default_output_ = nullptr;
    }
    // 登记表变化后在线设备与默认设备都要重新评选（波形与强度由随后的一次链路状态变化接管）
    emit outputReadyChanged();
    emit outputDevicesChanged();
    emit defaultDeviceChanged();
    refresh_limits();
    emit_authoritative_strength();
}

void DeviceController::notify_experimental_device(const QString& mac) {
    const QString normalized = mac.trimmed().toUpper();
    if (normalized.isEmpty()) {
        return;
    }
    auto& config = AppConfig::instance();
    const std::string key = "app.devices." + normalized.toStdString() + ".experimental_notice_shown";
    if (config.get_value<bool>(key, false)) {
        return;
    }
    config.set_value_with_name<bool>(key, true, "user");
    config.save_all();
    emit experimentalDeviceNotice(QStringLiteral("此设备连接为实验性，功能未经完整验证"));
}

bool DeviceController::output_available() const {
    return connected_ || ble_ready();
}

int DeviceController::channel_limit(const QString& channel) const {
    const EventChannelConfig config = limits_for(default_device_address(), channel);
    // 展示上限取 min(limit, safety)：与详情页写入同源，安全上限是输出强度的硬钳制
    const int limit = std::clamp(config.limit, 0, MAX_STRENGTH);
    const int safety = std::clamp(config.safety, 0, MAX_STRENGTH);
    return std::min(limit, safety);
}

EventChannelConfig DeviceController::limits_for(const QString& mac, const QString& channel) const {
    // 回退链的唯一实现：设备特有值 → app.channels.<通道>（含旧键迁移与内置默认）
    EventChannelConfig config = read_channel_config(channel.toStdString());
    const bool is_a = channel.compare(QStringLiteral("A"), Qt::CaseInsensitive) == 0;
    const bool is_b = channel.compare(QStringLiteral("B"), Qt::CaseInsensitive) == 0;
    const QString device = mac.trimmed().toUpper();
    if (device.isEmpty() || (!is_a && !is_b)) {
        return config;
    }
    // 设备特有键直接读配置：配置文件改动由配置系统重读后进入内存，这里不再另存快照
    const nlohmann::json section = device_section_of(device);
    device_section_value(section, is_a ? "limit_a" : "limit_b", &config.limit);
    device_section_value(section, is_a ? "safety_a" : "safety_b", &config.safety);
    return config;
}

QVariantMap DeviceController::limitsFor(const QString& mac, const QString& channel) const {
    const bool is_a = channel.compare(QStringLiteral("A"), Qt::CaseInsensitive) == 0;
    const bool is_b = channel.compare(QStringLiteral("B"), Qt::CaseInsensitive) == 0;
    const EventChannelConfig config = limits_for(mac, channel);
    int resolved = 0;
    // 来源标记只标该通道的两个键：任一键缺失即说明这部分仍取通道回退值
    const nlohmann::json section = (is_a || is_b) ? device_section_of(mac) : nlohmann::json::object();
    const bool has_limit = device_section_value(section, is_a ? "limit_a" : "limit_b", &resolved);
    const bool has_safety = device_section_value(section, is_a ? "safety_a" : "safety_b", &resolved);
    QVariantMap result;
    result.insert(QStringLiteral("limit"), config.limit);
    result.insert(QStringLiteral("safety"), config.safety);
    result.insert(QStringLiteral("effective"),
        std::min(std::clamp(config.limit, 0, MAX_STRENGTH), std::clamp(config.safety, 0, MAX_STRENGTH)));
    result.insert(QStringLiteral("device_limit"), has_limit);
    result.insert(QStringLiteral("device_safety"), has_safety);
    return result;
}

bool DeviceController::setDeviceLimit(const QString& mac, const QString& channel, const QString& kind, int value) {
    const bool is_a = channel.compare(QStringLiteral("A"), Qt::CaseInsensitive) == 0;
    const bool is_b = channel.compare(QStringLiteral("B"), Qt::CaseInsensitive) == 0;
    const bool is_limit = kind.compare(QStringLiteral("limit"), Qt::CaseInsensitive) == 0;
    const bool is_safety = kind.compare(QStringLiteral("safety"), Qt::CaseInsensitive) == 0;
    const QString device = mac.trimmed().toUpper();
    if (device.isEmpty() || (!is_a && !is_b) || (!is_limit && !is_safety)) {
        return false;
    }
    // 只写被编辑的那一个键：另一个键继续走通道回退，避免把回退值固化到设备上
    const QString key = QStringLiteral("app.devices.%1.%2%3")
        .arg(device, is_limit ? QStringLiteral("limit_") : QStringLiteral("safety_"),
            is_a ? QStringLiteral("a") : QStringLiteral("b"));
    const int clamped = std::clamp(value, 0, MAX_STRENGTH);
    auto& config = AppConfig::instance();
    config.set_value_with_name<int>(key.toStdString(), clamped, "user");
    config.save_all();
    LOG_MODULE("DeviceController", "setDeviceLimit", LOG_INFO,
        "设备通道上限已写入 " << key.toStdString() << "=" << clamped);
    // 默认设备的生效值可能随之变化：广播一次让看板与迷你窗跟上
    refresh_limits();
    return true;
}

QString DeviceController::default_device_address() const {
    const IOutputDevice* device = default_output();
    return device == nullptr ? QString() : device->device_address();
}

int DeviceController::default_device_index() const {
    const QString address = default_device_address();
    if (address.isEmpty()) {
        return -1;
    }
    const QVariantList devices = output_devices();
    for (int index = 0; index < devices.size(); ++index) {
        const QString candidate = devices.at(index).toMap().value(QStringLiteral("address")).toString();
        if (candidate.compare(address, Qt::CaseInsensitive) == 0) {
            return index;
        }
    }
    return -1;
}

QVariantList DeviceController::output_devices() const {
    QList<IOutputDevice*> devices = online_outputs();
    // 列表顺序即评选顺序：先按类型优先级（郊狼 > 负鼠 > 其它），同类型按先连接优先
    std::stable_sort(devices.begin(), devices.end(),
        [this](const IOutputDevice* left, const IOutputDevice* right) {
            const int left_priority = device_type_priority(left->device_type());
            const int right_priority = device_type_priority(right->device_type());
            if (left_priority != right_priority) {
                return left_priority < right_priority;
            }
            return connection_order(left) < connection_order(right);
        });
    QVariantList list;
    for (const IOutputDevice* device : devices) {
        const QString address = device->device_address();
        const QString label = device->device_label();
        QVariantMap item;
        item.insert(QStringLiteral("address"), address);
        item.insert(QStringLiteral("label"), label);
        item.insert(QStringLiteral("name"), label.isEmpty() ? address : label + QStringLiteral(" · ") + address);
        item.insert(QStringLiteral("model"), device->device_model());
        item.insert(QStringLiteral("type"), device->device_type());
        item.insert(QStringLiteral("battery"), device->battery());
        item.insert(QStringLiteral("order"), connection_order(device));
        item.insert(QStringLiteral("support"), static_cast<int>(device->support_state()));
        list.append(item);
    }
    return list;
}

bool DeviceController::wave_sending(const QString& channel) const {
    const int index = channel_index_of(channel);
    const IOutputDevice* device = default_output();
    if (index < 0 || device == nullptr) {
        return false;
    }
    return device->wave_sending(index + 1);
}

int DeviceController::wave_frame(const QString& channel) const {
    const int index = channel_index_of(channel);
    const IOutputDevice* device = default_output();
    if (index < 0 || device == nullptr) {
        return 0;
    }
    return device->wave_frame(index + 1);
}

int DeviceController::wave_frames(const QString& channel) const {
    const int index = channel_index_of(channel);
    const IOutputDevice* device = default_output();
    if (index < 0 || device == nullptr) {
        return 0;
    }
    return device->wave_frames(index + 1);
}

QObject* DeviceController::ip_selector() const {
    return ip_selector_;
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
    const int index = channel - 1;
    if (index < 0 || index > 1) {
        return;
    }
    // 规则侧波形登记：事件波形播放期间只登记不下发，事件播完后按登记恢复
    const bool event_active = event_wave_[index].active;
    rule_wave_[index].frames = pulses;
    rule_wave_[index].seconds = std::max(seconds, 1);
    rule_wave_[index].started_ms = event_active ? 0 : QDateTime::currentMSecsSinceEpoch();
    if (event_active) {
        LOG_MODULE("DeviceController", "sendWave", LOG_INFO,
            "事件波形播放中，规则波形已登记待恢复: 通道 " << ((index == 0) ? "A" : "B"));
        return;
    }
    if (IOutputDevice* device = active_output(index)) {
        device->play_wave(channel, pulses);
        QTimer* timer = wave_stop_timer(index);
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
    const int index = channel - 1;
    if (index == 0 || index == 1) {
        rule_wave_[index] = RuleWaveState();
    }
    if (IOutputDevice* device = active_output(index)) {
        QTimer* timer = wave_stop_timer(index);
        if (timer != nullptr) {
            timer->stop();
        }
        device->stop_wave(channel);
        return;
    }
    if (relay_ != nullptr) {
        relay_->send_clear(channel);
    }
}

QStringList DeviceController::outputTargets() const {
    return {QString::fromLatin1(kTargetCoyote)};
}

QString DeviceController::outputTarget(const QString& channel) const {
    const int index = channel_index_of(channel);
    if (index < 0) {
        return QString();
    }
    const auto& config = AppConfig::instance();
    const QString target = QString::fromStdString(
        config.get_value<std::string>(output_target_key(index).toStdString(), kTargetCoyote));
    // 配置值不在可选目标内时按默认目标处理
    return outputTargets().contains(target) ? target : QString::fromLatin1(kTargetCoyote);
}

void DeviceController::setOutputTarget(const QString& channel, const QString& target) {
    const int index = channel_index_of(channel);
    if (index < 0) {
        return;
    }
    if (!outputTargets().contains(target)) {
        report_error(QStringLiteral("未知输出目标: %1").arg(target));
        return;
    }
    if (outputTarget(channel) == target) {
        return;
    }
    // 切目标：先停旧目标波形（事件与规则波形状态一并清空），再落盘，最后按当前强度重发
    if (IOutputDevice* device = active_output(index)) {
        device->stop_wave(index + 1);
    }
    abort_event_wave(index);
    auto& config = AppConfig::instance();
    config.set_value<std::string>(output_target_key(index).toStdString(), target.toStdString());
    config.save_all();
    channel_output_[index].reset_output();
    update_channel_output(index);
    LOG_MODULE("DeviceController", "setOutputTarget", LOG_INFO,
        "通道 " << channel_name_of(index).toStdString() << " 输出目标切换为 " << target.toStdString());
}

void DeviceController::setDefaultDevice(const QString& mac) {
    const QString normalized = mac.trimmed().toUpper();
    auto& config = AppConfig::instance();
    config.set_value_with_name<std::string>("app.output.default_device", normalized.toStdString(), "user");
    config.save_all();
    LOG_MODULE("DeviceController", "setDefaultDevice", LOG_INFO,
        "默认设备切换为 " << (normalized.isEmpty() ? std::string("（自动评选）") : normalized.toStdString()));
    // 默认设备变化后界面上限与下发目标立即跟随
    emit defaultDeviceChanged();
    emit outputDevicesChanged();
    refresh_limits();
    for (int index = 0; index < 2; ++index) {
        channel_output_[index].reset_output();
        update_channel_output(index);
    }
}

void DeviceController::setDefaultDeviceByIndex(int index) {
    const QVariantList devices = output_devices();
    if (index < 0 || index >= devices.size()) {
        return;
    }
    setDefaultDevice(devices.at(index).toMap().value(QStringLiteral("address")).toString());
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

void DeviceController::apply_event_wave(const QString& channel, const QString& wave_name) {
    tick_event_wave(channel_index_of(channel), wave_name);
}

QString DeviceController::detect_lan_ip() const {
    return ip_selector_ != nullptr ? ip_selector_->active_ip() : QStringLiteral("127.0.0.1");
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
    return default_output() != nullptr;
}

QList<IOutputDevice*> DeviceController::online_outputs() const {
    QList<IOutputDevice*> list;
    // 注入的蓝牙设备逐个判定：已连接且具备输出能力者参与评选与下发
    for (IOutputDevice* device : outputs_) {
        if (device->connected() && device->output_capable()) {
            list.append(device);
        }
    }
    return list;
}

IOutputDevice* DeviceController::default_output() const {
    const QList<IOutputDevice*> online = online_outputs();
    if (online.isEmpty()) {
        return nullptr;
    }
    // 评选实现只有一份（pick_default_device）：用户覆盖 → 郊狼 > 负鼠 → 同类型先连接者
    QList<DefaultDeviceCandidate> candidates;
    for (const IOutputDevice* device : online) {
        DefaultDeviceCandidate candidate;
        candidate.address = device->device_address();
        candidate.type = device->device_type();
        candidate.order = connection_order(device);
        candidates.append(candidate);
    }
    // 用户覆盖值只在该设备在线时生效；离线（含用户主动断开）自动回到自动评选
    const QString configured = QString::fromStdString(AppConfig::instance().get_value<std::string>(
        "app.output.default_device", std::string()));
    const int index = pick_default_device(candidates, configured);
    return index < 0 ? nullptr : online.at(index);
}

int DeviceController::connection_order(const IOutputDevice* device) const {
    const auto it = connection_order_.constFind(device);
    // 未登记序号的设备排在同类型设备之后
    return it == connection_order_.constEnd() ? std::numeric_limits<int>::max() : it.value();
}

void DeviceController::notify_experimental_device(IOutputDevice* device) {
    if (device == nullptr || device->support_state() != IOutputDevice::SupportState::Experimental) {
        return;
    }
    notify_experimental_device(device->device_address());
}

void DeviceController::refresh_limits() {
    // 上下限来自配置系统（外部手改已由 ConfigManager 重读），这里只比较变化并广播
    const int limit_a = channel_limit(QStringLiteral("A"));
    const int limit_b = channel_limit(QStringLiteral("B"));
    if (limit_a == cached_limit_a_ && limit_b == cached_limit_b_) {
        return;
    }
    // 首次只记录当前值，不广播（避免启动期连带刷新整张看板）
    const bool first_read = cached_limit_a_ < 0 && cached_limit_b_ < 0;
    cached_limit_a_ = limit_a;
    cached_limit_b_ = limit_b;
    if (first_read) {
        return;
    }
    LOG_MODULE("DeviceController", "refresh_limits", LOG_INFO,
        "通道上限跟随配置更新: A=" << limit_a << " B=" << limit_b);
    emit limitsChanged();
}

IOutputDevice* DeviceController::active_output(int index) const {
    if (index < 0 || index > 1) {
        return nullptr;
    }
    // 每通道单选目标：本版唯一目标 coyote 对应默认输出设备
    if (outputTarget(channel_name_of(index)) != QString::fromLatin1(kTargetCoyote)) {
        return nullptr;
    }
    return default_output();
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
    if (IOutputDevice* device = active_output(channel - 1)) {
        device->set_strength(channel, mode, value);
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

void DeviceController::emit_authoritative_strength() {
    const IOutputDevice* device = default_output();
    const bool from_ble = device != nullptr;
    if (from_ble != strength_from_ble_) {
        strength_from_ble_ = from_ble;
        if (from_ble) {
            LOG_MODULE("DeviceController", "emit_authoritative_strength", LOG_INFO, "强度回执来源切换为蓝牙直连");
        }
        else {
            LOG_MODULE("DeviceController", "emit_authoritative_strength", LOG_INFO, "强度回执来源切换为内置中转");
        }
    }
    if (from_ble) {
        emit strengthFeedback(device->strength_a(), device->strength_b(),
            channel_limit(QStringLiteral("A")), channel_limit(QStringLiteral("B")));
        return;
    }
    emit strengthFeedback(relay_strength_a_, relay_strength_b_, relay_limit_a_, relay_limit_b_);
}

void DeviceController::tick_event_wave(int index, const QString& wave_name) {
    if (index < 0 || index > 1) {
        return;
    }
    const QStringList frames = wave_name.isEmpty() ? QStringList() : event_wave_frames(wave_name);
    const QStringList prior_frames = rule_wave_[index].frames;
    const int prior_seconds = rule_wave_remaining(index);
    const int frames_elapsed = event_wave_[index].active ? take_event_wave_writes(index) : 0;
    const EventWaveAction action = advance_event_wave(event_wave_[index], wave_name,
        static_cast<int>(frames.size()), output_target_ready(), prior_frames, prior_seconds, frames_elapsed);
    const std::string channel_text = (index == 0) ? "A" : "B";
    switch (action) {
    case EventWaveAction::NONE:
        return;
    case EventWaveAction::START:
    case EventWaveAction::REPLACE:
        // 事件波形优先：接管时丢弃规则侧登记，规则波形的停止计时不再生效
        rule_wave_[index] = RuleWaveState();
        last_refused_wave_[index].clear();
        start_event_wave(index, event_wave_[index].wave_name, frames);
        return;
    case EventWaveAction::RESTORE:
        restore_event_wave(index);
        return;
    case EventWaveAction::REFUSE:
        // 缺帧拒绝：同一波形只提示一次，通道状态保持不变
        if (last_refused_wave_[index] == wave_name) {
            return;
        }
        last_refused_wave_[index] = wave_name;
        LOG_MODULE("DeviceController", "tick_event_wave", LOG_WARN,
            "事件波形缺帧未下发: 通道 " << channel_text << " 波形《" << wave_name.toStdString() << "》 协议 "
                << WaveLibrary::instance().protocol().toStdString());
        report_error(QStringLiteral("事件波形《%1》缺少 %2 帧，未下发")
            .arg(wave_name, WaveLibrary::instance().protocol().toUpper()));
        return;
    case EventWaveAction::ABORT:
        abort_event_wave(index);
        return;
    }
}

QStringList DeviceController::event_wave_frames(const QString& wave_name) const {
    return WaveLibrary::instance().frames_of(wave_name.toStdString(), WaveLibrary::instance().protocol());
}

void DeviceController::start_event_wave(int index, const QString& wave_name, const QStringList& frames) {
    if (frames.isEmpty()) {
        return;
    }
    if (QTimer* timer = wave_stop_timer(index)) {
        timer->stop();
    }
    if (IOutputDevice* device = active_output(index)) {
        device->play_wave(index + 1, frames);
    }
    else if (relay_ != nullptr) {
        // 中转链路按帧数折算秒数：一帧 100ms，向上取整
        relay_->send_pulse(index + 1, frames, std::max(1, (static_cast<int>(frames.size()) + 9) / 10));
    }
    event_wave_writes_[index] = 0;
    LOG_MODULE("DeviceController", "start_event_wave", LOG_INFO,
        "事件波形开始播放: 通道 " << ((index == 0) ? "A" : "B") << " 波形《" << wave_name.toStdString()
            << "》 帧数 " << frames.size());
}

void DeviceController::restore_event_wave(int index) {
    // 事件播放期间登记的规则波形优先于接管前的波形
    const RuleWaveState deferred = rule_wave_[index];
    QStringList frames = deferred.frames;
    int seconds = deferred.seconds;
    rule_wave_[index] = RuleWaveState();
    if (frames.isEmpty() || seconds <= 0) {
        frames = event_wave_[index].prior_active ? event_wave_[index].prior_frames : QStringList();
        seconds = event_wave_[index].prior_active ? event_wave_[index].prior_seconds : 0;
    }
    clearQueue(index + 1);
    const std::string channel_text = (index == 0) ? "A" : "B";
    if (frames.isEmpty() || seconds <= 0) {
        LOG_MODULE("DeviceController", "restore_event_wave", LOG_INFO,
            "事件波形播放结束: 通道 " << channel_text << " 无接管前波形，已静音");
        return;
    }
    sendWave(index + 1, frames, seconds);
    LOG_MODULE("DeviceController", "restore_event_wave", LOG_INFO,
        "事件波形播放结束: 通道 " << channel_text << " 恢复接管前波形 帧数 " << frames.size() << " 时长 " << seconds
            << " 秒");
}

void DeviceController::abort_event_wave(int index) {
    if (index < 0 || index > 1) {
        return;
    }
    if (!event_wave_[index].active && rule_wave_[index].frames.isEmpty()) {
        return;
    }
    rule_wave_[index] = RuleWaveState();
    last_refused_wave_[index].clear();
    clearQueue(index + 1);
    LOG_MODULE("DeviceController", "abort_event_wave", LOG_INFO,
        "事件波形中止: 通道 " << ((index == 0) ? "A" : "B") << " 输出目标不可用，波形状态已清空");
}

int DeviceController::rule_wave_remaining(int index) const {
    if (index < 0 || index > 1) {
        return 0;
    }
    const RuleWaveState& rule = rule_wave_[index];
    if (rule.frames.isEmpty() || rule.seconds <= 0) {
        return 0;
    }
    // started_ms 为 0 表示事件波形期间暂存，恢复时按完整时长播放
    if (rule.started_ms <= 0) {
        return rule.seconds;
    }
    const qint64 elapsed_ms = QDateTime::currentMSecsSinceEpoch() - rule.started_ms;
    const int left = rule.seconds - static_cast<int>(elapsed_ms / 1000);
    return left > 0 ? left : 0;
}

QTimer* DeviceController::wave_stop_timer(int index) const {
    if (index == 0) {
        return ble_wave_timer_a_;
    }
    if (index == 1) {
        return ble_wave_timer_b_;
    }
    return nullptr;
}

int DeviceController::take_event_wave_writes(int index) {
    if (index < 0 || index > 1) {
        return 0;
    }
    if (!ble_ready()) {
        // 内置中转没有写入回执，按每帧一次推进
        return 1;
    }
    const int writes = event_wave_writes_[index];
    event_wave_writes_[index] = 0;
    return writes;
}
