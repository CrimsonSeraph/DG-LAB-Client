/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "CoyoteBleController.h"

#include "DebugLog.h"

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

} // namespace

CoyoteBleController::CoyoteBleController(QObject* parent)
    : QObject(parent)
    , write_char_uuid_(kWriteCharacteristic)
    , notify_char_uuid_(kNotifyCharacteristic) {
    tick_timer_ = std::make_unique<QTimer>(this);
    tick_timer_->setInterval(100);
    connect(tick_timer_.get(), &QTimer::timeout, this, [this]() {
        if (connected_ && ready_) {
            write_b0();
        }
    });
}

CoyoteBleController::~CoyoteBleController() {
    disconnectDevice();
}

void CoyoteBleController::initialize() {
    status_ = QStringLiteral("未连接");
    emit statusChanged();
    LOG_MODULE("CoyoteBleController", "initialize", LOG_INFO, "蓝牙控制器已就绪");
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

    // 加入 UI 列表
    QVariantMap item;
    QString displayName = info.name().isEmpty() ? QStringLiteral("郊狼设备(未知名称)") : info.name();
    item.insert(QStringLiteral("name"), displayName);
    item.insert(QStringLiteral("address"), address);
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

    pending_address_ = address;
    device_name_ = target.name().isEmpty() ? QStringLiteral("郊狼设备") : target.name();

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
                    if (state == QLowEnergyService::RemoteServiceDiscovered && battery_service_ != nullptr) {
                        const auto characteristic = battery_service_->characteristic(kBatteryCharacteristic);
                        if (characteristic.isValid()) {
                            battery_service_->readCharacteristic(characteristic);
                        }
                    }
                });
            connect(battery_service_, &QLowEnergyService::characteristicChanged, this,
                [this](const QLowEnergyCharacteristic&, const QByteArray& value) {
                    if (!value.isEmpty()) {
                        battery_ = static_cast<quint8>(value.at(0));
                        emit batteryChanged();
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
    connected_ = false;
    ready_ = false;
    strength_a_ = 0;
    strength_b_ = 0;
    battery_ = -1;
    tick_timer_->stop();
    emit stateChanged();
    emit strengthChanged();
    emit batteryChanged();
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
    tick_timer_->start();
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
    }
}

void CoyoteBleController::playWave(int channel, const QStringList& frames) {
    ChannelState& state = (channel == 1) ? channel_a_ : channel_b_;
    state.frames = frames;
    state.frame_index = 0;
}

void CoyoteBleController::stopWave(int channel) {
    ChannelState& state = (channel == 1) ? channel_a_ : channel_b_;
    state.frames.clear();
    state.frame_index = 0;
}

void CoyoteBleController::disconnectDevice() {
    tick_timer_->stop();
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
    scanning_ = false;
    connected_ = false;
    ready_ = false;
    emit stateChanged();
}
