/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "CoyoteBleController.h"

#include "DebugLog.h"
#include "OpossumBleController.h"

#include <QBluetoothDeviceDiscoveryAgent>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QPermissions>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>

namespace {

    // 郊狼 V3 蓝牙 UUID（16 位短 UUID，基址 0000xxxx-0000-1000-8000-00805f9b34fb）
    const QBluetoothUuid kCoyoteService(static_cast<quint16>(0x180C));
    const QBluetoothUuid kWriteCharacteristic(static_cast<quint16>(0x150A));
    const QBluetoothUuid kNotifyCharacteristic(static_cast<quint16>(0x150B));
    const QBluetoothUuid kBatteryService(static_cast<quint16>(0x180A));
    const QBluetoothUuid kBatteryCharacteristic(static_cast<quint16>(0x1500));

    const char* kSilentFrame = "0A0A0A0A00000000";

    // 蓝牙名前缀 → 型号。GATT 与郊狼 V3 相同的设备靠名字区分，未登记的型号一律不可连接
    struct ModelEntry {
        const char* prefix;  ///< 蓝牙名前缀
        const char* label;   ///< 型号显示名
        bool has_driver;     ///< 本版是否已实现驱动（可连接）
        bool verified;       ///< 驱动是否经实机验证（未验证按实验性展示）
        bool output;         ///< 是否具备输出能力（纯输入设备不参与默认设备评选）
        const char* type;    ///< 类型标识（默认设备优先级用）
    };
    constexpr ModelEntry kModelTable[] = {
        {"47L120", "爪印", false, false, false, "pawprint"},
        {"47L121", "郊狼 V3", true, true, true, "coyote"},
        {"47L124", "灵猫", false, false, false, "lynx"},
        {"47L127", "负鼠", true, false, true, "opossum"},
    };

    // 驱动支持程度：实机验证过的驱动为已支持，驱动已实现但未验证或未实现均为实验性
    IOutputDevice::SupportState support_state_of(bool has_driver, bool verified) {
        return (has_driver && verified) ? IOutputDevice::SupportState::Supported
                                        : IOutputDevice::SupportState::Experimental;
    }

    // 驱动支持程度的界面文案
    QString support_text_of(IOutputDevice::SupportState state) {
        if (state == IOutputDevice::SupportState::Supported) {
            return QStringLiteral("已支持");
        }
        if (state == IOutputDevice::SupportState::Experimental) {
            return QStringLiteral("实验性");
        }
        return QStringLiteral("暂不支持");
    }

} // namespace

CoyoteBleController::CoyoteBleController(QObject* parent)
    : IOutputDevice(parent)
    , write_char_uuid_(kWriteCharacteristic)
    , notify_char_uuid_(kNotifyCharacteristic) {
    // 接口信号转发既有信号：发射时机与次数不变，避免上层依赖具体实现
    connect(this, &CoyoteBleController::stateChanged, this, &IOutputDevice::outputStateChanged);
    connect(this, &CoyoteBleController::strengthChanged, this, &IOutputDevice::outputStrengthChanged);
    connect(this, &CoyoteBleController::waveStateChanged, this, &IOutputDevice::outputWaveChanged);
    tick_timer_ = std::make_unique<QTimer>(this);
    tick_timer_->setInterval(100);
    connect(tick_timer_.get(), &QTimer::timeout, this, [this]() {
        if (connected_ && ready_) {
            write_b0();
        }
    });
    battery_timer_ = std::make_unique<QTimer>(this);
    battery_timer_->setInterval(60000);
    connect(battery_timer_.get(), &QTimer::timeout, this, [this]() { read_battery(); });
}

CoyoteBleController::~CoyoteBleController() {
    disconnectDevice();
}

void CoyoteBleController::initialize() {
    status_ = QStringLiteral("未连接");
    emit statusChanged();
    LOG_MODULE("CoyoteBleController", "initialize", LOG_INFO, "蓝牙控制器已就绪");
}

CoyoteBleController::DeviceProfile CoyoteBleController::identify(const QString& name) {
    DeviceProfile profile;
    for (const ModelEntry& entry : kModelTable) {
        const QString prefix = QString::fromLatin1(entry.prefix);
        if (name.startsWith(prefix)) {
            profile.model = prefix;
            profile.label = QString::fromUtf8(entry.label);
            profile.supported = entry.has_driver;
            profile.output = entry.output;
            profile.type = QString::fromLatin1(entry.type);
            profile.state = support_state_of(entry.has_driver, entry.verified);
            return profile;
        }
    }
    if (name.startsWith(QStringLiteral("47L1"))) {
        // 未登记的 47L1 前缀：型号段取前 6 位，标注暂不支持
        profile.model = name.left(6);
        profile.label = QStringLiteral("未知型号");
        profile.state = SupportState::Unsupported;
        return profile;
    }
    // 非 47L1 前缀：型号未知但沿用既有行为允许连接，按实验性列出（功能未经完整验证）
    profile.supported = true;
    profile.label = QStringLiteral("未知型号");
    profile.state = SupportState::Experimental;
    return profile;
}

void CoyoteBleController::attach_opossum(OpossumBleController* opossum) {
    opossum_ = opossum;
    if (opossum_ == nullptr) {
        return;
    }
    // 驱动先于本类销毁时清空指针：本类析构仍会走 disconnectDevice()
    connect(opossum_, &QObject::destroyed, this, [this]() { opossum_ = nullptr; });
    // 只有负鼠成为界面当前链路时才广播连接变化，避免郊狼离线期间多余的复位
    connect(opossum_, &OpossumBleController::stateChanged, this, [this]() {
        const bool active = opossum_active();
        if (active == opossum_link_shown_) {
            return;
        }
        opossum_link_shown_ = active;
        emit stateChanged();
        emit waveStateChanged();
    });
    connect(opossum_, &OpossumBleController::strengthChanged, this, &CoyoteBleController::strengthChanged);
    connect(opossum_, &OpossumBleController::batteryChanged, this, &CoyoteBleController::batteryChanged);
    connect(opossum_, &OpossumBleController::screenSyncChanged, this, &CoyoteBleController::screenSyncChanged);
    connect(opossum_, &OpossumBleController::statusChanged, this, [this]() {
        if (opossum_active()) {
            emit statusChanged();
        }
    });
}

// 负鼠是否为界面上的当前链路（郊狼未连接且负鼠已连接）
bool CoyoteBleController::opossum_active() const {
    return !connected_ && opossum_ != nullptr && opossum_->connected();
}

bool CoyoteBleController::link_connected() const {
    return connected_ || opossum_active();
}

QString CoyoteBleController::link_device_name() const {
    if (opossum_active()) {
        const QString label = opossum_->device_label();
        return label.isEmpty() ? opossum_->device_address() : label;
    }
    return device_name_;
}

int CoyoteBleController::link_battery() const {
    return opossum_active() ? opossum_->battery() : battery_;
}

int CoyoteBleController::link_strength_a() const {
    return opossum_active() ? opossum_->strength_a() : strength_a_;
}

int CoyoteBleController::link_strength_b() const {
    return opossum_active() ? opossum_->strength_b() : strength_b_;
}

QString CoyoteBleController::link_status() const {
    return opossum_active() ? opossum_->status() : status_;
}

bool CoyoteBleController::screen_sync_available() const {
    return opossum_active() ? opossum_->screen_sync_available() : true;
}

void CoyoteBleController::set_status(const QString& text) {
    if (status_ == text) {
        return;
    }
    status_ = text;
    emit statusChanged();
}

void CoyoteBleController::do_start_scan() {
    if (scanning_) return;

    // 清理旧数据
    devices_.clear();
    device_cache_.clear();
    emit devicesChanged();

    // 创建发现代理
    discovery_ = new QBluetoothDeviceDiscoveryAgent(this);
    discovery_->setLowEnergyDiscoveryTimeout(15000);

    connect(discovery_, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
        this, &CoyoteBleController::on_device_discovered);
    connect(discovery_, &QBluetoothDeviceDiscoveryAgent::finished,
        this, &CoyoteBleController::on_scan_finished);
    connect(discovery_, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
        [this](QBluetoothDeviceDiscoveryAgent::Error error) {
            scanning_ = false;
            emit stateChanged();
            set_status(QStringLiteral("蓝牙扫描失败，错误码: %1").arg(static_cast<int>(error)));
            emit errorOccurred(QStringLiteral("蓝牙扫描失败，请检查权限和蓝牙开关"));
        });

    scanning_ = true;
    emit stateChanged();
    set_status(QStringLiteral("正在扫描附近的郊狼设备…"));
    discovery_->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

QByteArray CoyoteBleController::frame_for_channel(const QStringList& frames, int index) const {
    if (frames.isEmpty()) {
        return QByteArray::fromHex(kSilentFrame);
    }
    const QByteArray bytes = QByteArray::fromHex(frames.at(index % frames.size()).toLatin1());
    return bytes.size() == 8 ? bytes : QByteArray::fromHex(kSilentFrame);
}

const CoyoteBleController::ChannelState& CoyoteBleController::state_of(int channel) const {
    return (channel == 1) ? channel_a_ : channel_b_;
}

bool CoyoteBleController::wave_sending(int channel) const {
    return !state_of(channel).frames.isEmpty();
}

int CoyoteBleController::wave_frame(int channel) const {
    const ChannelState& state = state_of(channel);
    if (state.frames.isEmpty()) {
        return 0;
    }
    // frame_index 指向下一帧，已下发的那一帧是它的前一格（1 起计数）
    return (state.frame_index + state.frames.size() - 1) % state.frames.size() + 1;
}

int CoyoteBleController::wave_frames(int channel) const {
    return state_of(channel).frames.size();
}

void CoyoteBleController::startScan() {
    if (scanning_) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QBluetoothPermission permission;
    permission.setCommunicationModes(QBluetoothPermission::Access);

    qApp->requestPermission(permission, [this](const QPermission& perm) {
        if (perm.status() == Qt::PermissionStatus::Granted) {
            do_start_scan();
        }
        else {
            set_status(QStringLiteral("蓝牙权限被拒绝"));
        }
    });
    return;
#else
    do_start_scan();
#endif
}

void CoyoteBleController::on_device_discovered(const QBluetoothDeviceInfo& info) {
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
        return;
    }
    // 使用地址作为唯一标识去重
    QString address = info.address().toString();
    if (device_cache_.contains(address)) {
        return;
    }

    // 缓存完整的 QBluetoothDeviceInfo
    device_cache_.insert(address, info);

    // 加入 UI 列表：行上带「已支持 / 实验性 / 暂不支持」三态标签
    QVariantMap item;
    QString displayName = info.name().isEmpty() ? QStringLiteral("郊狼设备(未知名称)") : info.name();
    const DeviceProfile profile = identify(info.name());
    item.insert(QStringLiteral("name"), displayName);
    item.insert(QStringLiteral("address"), address);
    item.insert(QStringLiteral("supported"), profile.supported);
    item.insert(QStringLiteral("support"), static_cast<int>(profile.state));
    item.insert(QStringLiteral("supportText"), support_text_of(profile.state));
    item.insert(QStringLiteral("connectable"), profile.state != SupportState::Unsupported);
    item.insert(QStringLiteral("output"), profile.output);
    // 行内提示：实验性型号区分「驱动已实现未验证」与「驱动尚未实现」，未知型号给出可连接的型号段
    const QString model_text = profile.model.isEmpty() ? QString() : QStringLiteral("型号 %1，").arg(profile.model);
    QString hint;
    if (profile.state == SupportState::Experimental) {
        hint = profile.supported ? QStringLiteral("%1实验性驱动已实现，未经实机验证")
                                       .arg(model_text)
                                 : QStringLiteral("暂不可连接（%1驱动尚未实现）").arg(model_text);
    }
    else if (profile.state == SupportState::Unsupported) {
        hint = QStringLiteral("不可连接（%1型号未登记）").arg(model_text);
    }
    item.insert(QStringLiteral("hint"), hint);
    devices_.append(item);
    emit devicesChanged();
}

void CoyoteBleController::on_scan_finished() {
    scanning_ = false;
    emit stateChanged();
    set_status(devices_.isEmpty() ? QStringLiteral("未发现郊狼设备") : QStringLiteral("扫描完成，请选择设备"));
}

void CoyoteBleController::connectDevice(const QString& address) {
    if (address.isEmpty()) {
        return;
    }
    QBluetoothDeviceInfo target = device_cache_.value(address);

    if (!target.isValid()) {
        set_status(QStringLiteral("设备信息已过期，请重新扫描"));
        return;
    }

    const DeviceProfile profile = identify(target.name());
    if (profile.state == SupportState::Unsupported) {
        // 型号未登记：GATT 相同但指令含义不同，拒绝连接避免误写
        LOG_MODULE("CoyoteBleController", "connectDevice", LOG_WARN,
            "拒绝连接未支持型号:" << target.name().toStdString() << " 型号 " << profile.model.toStdString());
        set_status(QStringLiteral("暂不支持该设备（型号 %1）").arg(profile.model));
        emit errorOccurred(QStringLiteral("暂不支持该设备（型号 %1）").arg(profile.model));
        return;
    }
    if (profile.type == QStringLiteral("opossum")) {
        // 负鼠：GATT 相同但指令集不同，写入交给负鼠驱动；扫描与连接入口仍走本类
        if (opossum_ == nullptr) {
            LOG_MODULE("CoyoteBleController", "connectDevice", LOG_WARN, "负鼠驱动未接入，拒绝连接");
            set_status(QStringLiteral("负鼠驱动未接入"));
            emit errorOccurred(QStringLiteral("负鼠驱动未接入，本版无法连接该设备"));
            return;
        }
        if (opossum_->connected()) {
            set_status(QStringLiteral("负鼠已连接"));
            return;
        }
        // 同一时间只维持一条蓝牙链路：连接负鼠前先断开当前链路
        if (connected_) {
            disconnectDevice();
        }
        opossum_->connect_device(target);
        set_status(QStringLiteral("正在连接 %1…").arg(profile.label));
        return;
    }
    if (profile.state == SupportState::Experimental) {
        // 实验性型号：行可点选，但驱动未实现，明确告知而不是假装可用
        LOG_MODULE("CoyoteBleController", "connectDevice", LOG_WARN,
            "实验性型号驱动尚未实现:" << target.name().toStdString() << " 型号 " << profile.model.toStdString());
        set_status(QStringLiteral("%1 驱动尚未实现").arg(profile.label));
        emit errorOccurred(QStringLiteral("%1（型号 %2）驱动尚未实现，本版无法连接")
            .arg(profile.label, profile.model));
        return;
    }
    // 同一时间只维持一条蓝牙链路：连接郊狼前先断开负鼠
    if (opossum_ != nullptr && opossum_->connected()) {
        opossum_->disconnect_device();
    }

    pending_address_ = address;
    device_name_ = target.name().isEmpty() ? QStringLiteral("郊狼设备") : target.name();
    device_address_ = address;
    device_label_ = profile.label.isEmpty() ? device_name_ : profile.label;
    device_model_ = profile.model;
    device_type_ = profile.type;
    device_output_capable_ = profile.output;
    support_state_ = profile.state;

    // 用完整的 target 对象创建控制器
    controller_ = QLowEnergyController::createCentral(target, this);

    connect(controller_, &QLowEnergyController::connected, this, &CoyoteBleController::on_controller_connected);
    connect(controller_, &QLowEnergyController::disconnected, this, &CoyoteBleController::on_controller_disconnected);
    connect(controller_, &QLowEnergyController::serviceDiscovered, this, [](const QBluetoothUuid&) {});
    connect(controller_, &QLowEnergyController::discoveryFinished, this, [this]() {
        if (controller_ == nullptr) {
            return;
        }
        coyote_service_ = controller_->createServiceObject(kCoyoteService, this);
        if (coyote_service_ != nullptr) {
            connect(coyote_service_, &QLowEnergyService::stateChanged,
                this, &CoyoteBleController::on_service_state_changed);
            connect(coyote_service_, &QLowEnergyService::characteristicChanged,
                this, &CoyoteBleController::on_characteristic_changed);
            coyote_service_->discoverDetails();
        }
        connect(coyote_service_, QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::errorOccurred),
            this, [this](QLowEnergyService::ServiceError error) {
                LOG_MODULE("CoyoteBleController", "connectDevice", LOG_ERROR, "服务错误:" << error);
                set_status(QStringLiteral("蓝牙服务错误: %1").arg(error));
            });
        connect(coyote_service_, &QLowEnergyService::characteristicWritten,
            this, [](const QLowEnergyCharacteristic& c, const QByteArray& value) {
                LOG_MODULE("CoyoteBleController", "connectDevice", LOG_INFO,
                    "写入成功:" << c.uuid().toString().toStdString() << " " << value.toHex().toStdString());
            });
        battery_service_ = controller_->createServiceObject(kBatteryService, this);
        if (battery_service_ != nullptr) {
            connect(battery_service_, &QLowEnergyService::stateChanged, this,
                [this](QLowEnergyService::ServiceState state) {
                    if (state == QLowEnergyService::RemoteServiceDiscovered) {
                        subscribe_battery();
                    }
                });
            connect(battery_service_, &QLowEnergyService::characteristicChanged, this,
                [this](const QLowEnergyCharacteristic& characteristic, const QByteArray& value) {
                    if (characteristic.uuid() == kBatteryCharacteristic) {
                        apply_battery_value(value);
                    }
                });
            // 主动读取的回包同样带电量：不接这个信号读回值会被丢弃
            connect(battery_service_, &QLowEnergyService::characteristicRead, this,
                [this](const QLowEnergyCharacteristic& characteristic, const QByteArray& value) {
                    if (characteristic.uuid() == kBatteryCharacteristic) {
                        apply_battery_value(value);
                    }
                });
            battery_service_->discoverDetails();
        }
    });
    connect(controller_, &QLowEnergyController::errorOccurred, this,
        [this](QLowEnergyController::Error) {
            set_status(QStringLiteral("蓝牙连接失败"));
        });

    set_status(QStringLiteral("正在连接 %1…").arg(device_name_));
    controller_->connectToDevice();
}

void CoyoteBleController::on_controller_connected() {
    // 重连时旧控制器可能补发信号，只认当前控制器的事件
    if (sender() != controller_) {
        return;
    }
    connected_ = true;
    emit stateChanged();
    set_status(QStringLiteral("已连接 %1，正在发现服务…").arg(device_name_));
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
        channel->pending_mode = 0;
        channel->waiting_ack = false;
    }
    if (controller_ != nullptr) {
        controller_->discoverServices();
    }
}

void CoyoteBleController::on_controller_disconnected() {
    // 重连时旧控制器可能补发信号，只认当前控制器的事件
    if (sender() != controller_) {
        return;
    }
    LOG_MODULE("CoyoteBleController", "on_controller_disconnected", LOG_INFO, "蓝牙连接已断开");
    connected_ = false;
    ready_ = false;
    strength_a_ = 0;
    strength_b_ = 0;
    battery_ = -1;
    tick_timer_->stop();
    battery_timer_->stop();
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
    }
    emit stateChanged();
    emit strengthChanged();
    emit batteryChanged();
    emit waveStateChanged();
    set_status(QStringLiteral("蓝牙已断开"));
}

void CoyoteBleController::on_service_state_changed(QLowEnergyService::ServiceState state) {
    if (state == QLowEnergyService::RemoteServiceDiscovering) {
        return;
    }
    if (state != QLowEnergyService::RemoteServiceDiscovered) {
        return;
    }
    ensure_characteristics();
}

void CoyoteBleController::ensure_characteristics() {
    if (coyote_service_ == nullptr) {
        return;
    }
    const auto write_char = coyote_service_->characteristic(write_char_uuid_);
    const auto notify_char = coyote_service_->characteristic(notify_char_uuid_);

    LOG_MODULE("CoyoteBleController", "ensure_characteristics", LOG_DEBUG, "写特征是否有效:" << write_char.isValid());
    LOG_MODULE("CoyoteBleController", "ensure_characteristics", LOG_DEBUG, "通知特征是否有效:" << notify_char.isValid());

    if (!write_char.isValid() || !notify_char.isValid()) {
        set_status(QStringLiteral("未找到郊狼写入/通知特征"));
        return;
    }
    // 写入 CCCD 打开通知（0x0100 小端）
    const auto cccd = notify_char.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    if (cccd.isValid()) {
        coyote_service_->writeDescriptor(cccd, QByteArray::fromHex("0100"));
    }

    ready_ = true;
    emit stateChanged();
    set_status(QStringLiteral("已连接 %1").arg(device_name_));
    applyLimits(limit_a_, limit_b_, 128, 128);
    // 重连后重发上次绝对强度：设备不会保留断连前的目标值
    if (last_set_a_ >= 0) {
        setStrength(1, 2, last_set_a_);
    }
    if (last_set_b_ >= 0) {
        setStrength(2, 2, last_set_b_);
    }
    tick_timer_->start();
}

void CoyoteBleController::subscribe_battery() {
    if (battery_service_ == nullptr) {
        return;
    }
    const auto characteristic = battery_service_->characteristic(kBatteryCharacteristic);
    if (!characteristic.isValid()) {
        LOG_MODULE("CoyoteBleController", "subscribe_battery", LOG_WARN, "未找到电量特征");
        return;
    }
    // 支持 Notify 时写入 CCCD 订阅电量变化，让电量能随时间刷新
    const auto cccd = characteristic.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    if (cccd.isValid() && (characteristic.properties() & QLowEnergyCharacteristic::Notify)) {
        battery_service_->writeDescriptor(cccd, QByteArray::fromHex("0100"));
    }
    read_battery();
    battery_timer_->start();
}

void CoyoteBleController::read_battery() {
    if (battery_service_ == nullptr || !connected_) {
        return;
    }
    const auto characteristic = battery_service_->characteristic(kBatteryCharacteristic);
    if (!characteristic.isValid()) {
        return;
    }
    battery_service_->readCharacteristic(characteristic);
}

void CoyoteBleController::apply_battery_value(const QByteArray& value) {
    if (value.isEmpty()) {
        return;
    }
    const int percent = static_cast<quint8>(value.at(0));
    LOG_MODULE("CoyoteBleController", "apply_battery_value", LOG_INFO,
        "电量回传:" << value.toHex().toStdString() << " → " << percent << "%");
    if (percent == battery_) {
        return;
    }
    battery_ = percent;
    emit batteryChanged();
}

void CoyoteBleController::on_characteristic_changed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() == kBatteryCharacteristic) {
        if (!value.isEmpty()) {
            battery_ = static_cast<quint8>(value.at(0));
            emit batteryChanged();
        }
        return;
    }
    if (!value.isEmpty() && static_cast<quint8>(value.at(0)) == 0x53) {
        // 设备自报身份帧（0x53 + MAC）：只记一次，避免 100ms 级刷屏
        if (!identity_frame_logged_) {
            identity_frame_logged_ = true;
            LOG_MODULE("CoyoteBleController", "on_characteristic_changed", LOG_DEBUG,
                "设备身份帧:" << value.toHex().toStdString());
        }
        return;
    }
    if (value.size() >= 4 && static_cast<quint8>(value.at(0)) == 0xB1) {
        const int seq = static_cast<quint8>(value.at(1));
        strength_a_ = static_cast<quint8>(value.at(2));
        strength_b_ = static_cast<quint8>(value.at(3));
        if (channel_a_.waiting_ack && seq == channel_a_.pending_seq) {
            channel_a_.waiting_ack = false;
        }
        if (channel_b_.waiting_ack && seq == channel_b_.pending_seq) {
            channel_b_.waiting_ack = false;
        }
        emit strengthChanged();
    }
}

QByteArray CoyoteBleController::build_b0() {
    int a_mode = 0;
    int b_mode = 0;
    int a_value = 0;
    int b_value = 0;
    bool has_op = false;

    if (channel_a_.pending_mode != 0 && !channel_a_.waiting_ack) {
        a_mode = channel_a_.pending_mode;
        a_value = channel_a_.pending_value;
        has_op = true;
    }
    if (channel_b_.pending_mode != 0 && !channel_b_.waiting_ack) {
        b_mode = channel_b_.pending_mode;
        b_value = channel_b_.pending_value;
        has_op = true;
    }

    int seq = 0;
    if (has_op) {
        seq = ++sequence_;
        if (seq > 15) {
            sequence_ = 1;
            seq = 1;
        }
    }

    const QByteArray frame_a = frame_for_channel(channel_a_.frames, channel_a_.frame_index);
    const QByteArray frame_b = frame_for_channel(channel_b_.frames, channel_b_.frame_index);

    QByteArray data;
    data.append(static_cast<char>(0xB0));
    data.append(static_cast<char>(((seq & 0x0F) << 4) | ((a_mode & 0x03) << 2) | (b_mode & 0x03)));
    data.append(static_cast<char>(a_value & 0xFF));
    data.append(static_cast<char>(b_value & 0xFF));
    data.append(frame_a.mid(0, 4));
    data.append(frame_a.mid(4, 4));
    data.append(frame_b.mid(0, 4));
    data.append(frame_b.mid(4, 4));
    return data;
}

void CoyoteBleController::write_b0() {
    if (coyote_service_ == nullptr) {
        return;
    }
    const auto write_char = coyote_service_->characteristic(write_char_uuid_);
    if (!write_char.isValid()) {
        return;
    }

    const QByteArray data = build_b0();
    coyote_service_->writeCharacteristic(write_char, data, QLowEnergyService::WriteWithoutResponse);

    // 记录本 tick 使用的序号，等待 B1 回执后再接受下一次强度输入
    const int seq = (static_cast<quint8>(data.at(1)) >> 4) & 0x0F;
    if (seq != 0) {
        if (channel_a_.pending_mode != 0 && !channel_a_.waiting_ack) {
            channel_a_.waiting_ack = true;
            channel_a_.pending_seq = seq;
            channel_a_.pending_mode = 0;
        }
        if (channel_b_.pending_mode != 0 && !channel_b_.waiting_ack) {
            channel_b_.waiting_ack = true;
            channel_b_.pending_seq = seq;
            channel_b_.pending_mode = 0;
        }
    }

    if (!channel_a_.frames.isEmpty()) {
        channel_a_.frame_index = (channel_a_.frame_index + 1) % channel_a_.frames.size();
    }
    if (!channel_b_.frames.isEmpty()) {
        channel_b_.frame_index = (channel_b_.frame_index + 1) % channel_b_.frames.size();
    }
    if (!channel_a_.frames.isEmpty() || !channel_b_.frames.isEmpty()) {
        emit waveStateChanged();
    }
}

void CoyoteBleController::write_bf() {
    if (coyote_service_ == nullptr) {
        return;
    }
    const auto write_char = coyote_service_->characteristic(write_char_uuid_);
    if (!write_char.isValid()) {
        return;
    }
    QByteArray data;
    data.append(static_cast<char>(0xBF));
    data.append(static_cast<char>(limit_a_ & 0xFF));
    data.append(static_cast<char>(limit_b_ & 0xFF));
    data.append(static_cast<char>(128));
    data.append(static_cast<char>(128));
    data.append(static_cast<char>(128));
    data.append(static_cast<char>(128));
    coyote_service_->writeCharacteristic(write_char, data, QLowEnergyService::WriteWithResponse);
}

void CoyoteBleController::applyLimits(int limitA, int limitB, int freqBalance, int strengthBalance) {
    limit_a_ = std::clamp(limitA, 0, 200);
    limit_b_ = std::clamp(limitB, 0, 200);
    Q_UNUSED(freqBalance);
    Q_UNUSED(strengthBalance);
    write_bf();
}

void CoyoteBleController::setStrength(int channel, int mode, int value) {
    // 负鼠为当前链路时强度转交负鼠驱动（B3 只有绝对强度，相对增减由驱动折算）
    if (opossum_active()) {
        opossum_->set_strength(channel, mode, value);
        return;
    }
    ChannelState& state = (channel == 1) ? channel_a_ : channel_b_;
    if (mode == 3 || mode == 4) {
        // 连续增减：按次数转换为单次增减（沿用规则引擎的 0/1/2 语义）
        state.pending_mode = (mode == 3) ? 2 : 1;
        state.pending_value = 1;
        return;
    }
    if (mode == 0) {
        state.pending_mode = 2;
        state.pending_value = std::max(1, value);
    }
    else if (mode == 1) {
        state.pending_mode = 1;
        state.pending_value = std::max(1, value);
    }
    else {
        state.pending_mode = 3;
        state.pending_value = std::clamp(value, 0, 200);
        // 记录最后一条绝对强度：重连后按它重发，避免界面值与设备实际强度脱节
        if (channel == 1) {
            last_set_a_ = state.pending_value;
        }
        else {
            last_set_b_ = state.pending_value;
        }
    }
}

void CoyoteBleController::playWave(int channel, const QStringList& frames) {
    ChannelState& state = (channel == 1) ? channel_a_ : channel_b_;
    state.frames = frames;
    state.frame_index = 0;
    emit waveStateChanged();
}

void CoyoteBleController::stopWave(int channel) {
    ChannelState& state = (channel == 1) ? channel_a_ : channel_b_;
    state.frames.clear();
    state.frame_index = 0;
    emit waveStateChanged();
}

void CoyoteBleController::disconnectDevice() {
    tick_timer_->stop();
    battery_timer_->stop();
    if (opossum_ != nullptr && opossum_->connected()) {
        opossum_->disconnect_device();
    }
    if (controller_ != nullptr) {
        controller_->disconnectFromDevice();
        controller_->deleteLater();
        controller_ = nullptr;
    }
    coyote_service_ = nullptr;
    battery_service_ = nullptr;
    if (discovery_ != nullptr) {
        discovery_->stop();
        discovery_ = nullptr;
    }
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
    }
    scanning_ = false;
    connected_ = false;
    ready_ = false;
    // 设备标识随连接失效：离线设备不参与默认设备评选，配置键仍在下次连接后继续生效
    device_address_.clear();
    device_label_.clear();
    device_model_.clear();
    device_type_ = QStringLiteral("unknown");
    device_output_capable_ = true;
    support_state_ = SupportState::Unsupported;
    emit stateChanged();
    emit waveStateChanged();
}
