/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "OpossumBleController.h"

#include "DebugLog.h"
#include "OpossumFrames.h"

#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyDescriptor>
#include <QLowEnergyService>
#include <QTimer>

#include <algorithm>

namespace {

    // 负鼠振动控制器蓝牙 UUID（与郊狼 V3 相同的 16 位短 UUID）
    const QBluetoothUuid kPrimaryService(static_cast<quint16>(0x180C));
    const QBluetoothUuid kWriteCharacteristic(static_cast<quint16>(0x150A));
    const QBluetoothUuid kNotifyCharacteristic(static_cast<quint16>(0x150B));
    const QBluetoothUuid kBatteryService(static_cast<quint16>(0x180A));
    const QBluetoothUuid kBatteryCharacteristic(static_cast<quint16>(0x1500));

    constexpr int kB2AckTimeoutMs = 1500; ///< B2 写入回执等待上限

} // namespace

OpossumBleController::OpossumBleController(QObject* parent)
    : IOutputDevice(parent)
    , write_char_uuid_(kWriteCharacteristic)
    , notify_char_uuid_(kNotifyCharacteristic) {
    // 接口信号由本类既有信号转发：连接与强度回执的发射时机不变
    connect(this, &OpossumBleController::stateChanged, this, &IOutputDevice::outputStateChanged);
    connect(this, &OpossumBleController::strengthChanged, this, &IOutputDevice::outputStrengthChanged);
    tick_timer_ = std::make_unique<QTimer>(this);
    tick_timer_->setInterval(100);
    connect(tick_timer_.get(), &QTimer::timeout, this, [this]() { write_b0(); });
    battery_timer_ = std::make_unique<QTimer>(this);
    battery_timer_->setInterval(60000);
    connect(battery_timer_.get(), &QTimer::timeout, this, [this]() { read_battery(); });
    b2_ack_timer_ = std::make_unique<QTimer>(this);
    b2_ack_timer_->setSingleShot(true);
    b2_ack_timer_->setInterval(kB2AckTimeoutMs);
    connect(b2_ack_timer_.get(), &QTimer::timeout, this, [this]() { on_b2_ack_timeout(); });
}

OpossumBleController::~OpossumBleController() {
    disconnect_device();
}

void OpossumBleController::initialize() {
    status_ = QStringLiteral("未连接");
    emit statusChanged();
    LOG_MODULE("OpossumBleController", "initialize", LOG_INFO, "负鼠驱动已就绪（实验性）");
}

void OpossumBleController::connect_device(const QBluetoothDeviceInfo& info) {
    if (!info.isValid() || controller_ != nullptr) {
        return;
    }
    device_address_ = info.address().toString();
    device_model_ = QStringLiteral("47L127");
    device_label_ = QStringLiteral("负鼠");
    controller_ = QLowEnergyController::createCentral(info, this);
    connect(controller_, &QLowEnergyController::connected, this, &OpossumBleController::on_controller_connected);
    connect(controller_, &QLowEnergyController::disconnected, this, &OpossumBleController::on_controller_disconnected);
    connect(controller_, &QLowEnergyController::discoveryFinished, this, [this]() {
        if (controller_ == nullptr) {
            return;
        }
        primary_service_ = controller_->createServiceObject(kPrimaryService, this);
        if (primary_service_ != nullptr) {
            connect(primary_service_, &QLowEnergyService::stateChanged, this,
                &OpossumBleController::on_service_state_changed);
            connect(primary_service_, &QLowEnergyService::characteristicChanged, this,
                &OpossumBleController::on_characteristic_changed);
            connect(primary_service_, &QLowEnergyService::characteristicWritten, this,
                &OpossumBleController::on_write_completed);
            connect(primary_service_, QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::errorOccurred),
                this, [this](QLowEnergyService::ServiceError error) { on_service_error(static_cast<int>(error)); });
            primary_service_->discoverDetails();
        }
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
    connect(controller_, &QLowEnergyController::errorOccurred, this, [this](QLowEnergyController::Error) {
        set_status(QStringLiteral("负鼠蓝牙连接失败"));
    });
    set_status(QStringLiteral("正在连接负鼠…"));
    controller_->connectToDevice();
}

void OpossumBleController::disconnect_device() {
    tick_timer_->stop();
    battery_timer_->stop();
    b2_ack_timer_->stop();
    b2_pending_ack_ = false;
    if (controller_ != nullptr) {
        controller_->disconnectFromDevice();
        controller_->deleteLater();
        controller_ = nullptr;
    }
    primary_service_ = nullptr;
    battery_service_ = nullptr;
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
    }
    const bool was_connected = connected_;
    connected_ = false;
    ready_ = false;
    battery_ = -1;
    strength_a_ = 0;
    strength_b_ = 0;
    // 设备标识随连接失效：离线设备不参与默认设备评选，配置键仍在下次连接后继续生效
    device_address_.clear();
    device_label_.clear();
    device_model_.clear();
    if (was_connected) {
        emit stateChanged();
        emit strengthChanged();
        emit batteryChanged();
        emit outputWaveChanged();
        set_status(QStringLiteral("负鼠已断开"));
    }
    else {
        status_ = QStringLiteral("未连接");
        emit statusChanged();
    }
}

bool OpossumBleController::wave_sending(int channel) const {
    return !state_of(channel).frames.isEmpty();
}

int OpossumBleController::wave_frame(int channel) const {
    const ChannelState& state = state_of(channel);
    if (state.frames.isEmpty()) {
        return 0;
    }
    // frame_index 指向下一帧，已下发的那一帧是它的前一格（1 起计数）
    // 收窄依据：取模结果必小于帧数，编辑器限制单通道帧数 600（时长 60000ms / 每帧 100ms），库文件超出该上限也仍远小于 INT_MAX；返回值上游是 QML 的 int 属性
    return static_cast<int>((state.frame_index + state.frames.size() - 1) % state.frames.size() + 1);
}

int OpossumBleController::wave_frames(int channel) const {
    // 收窄依据：编辑器限制单通道帧数 600（时长 60000ms / 每帧 100ms），库文件超出该上限也仍远小于 INT_MAX；返回值上游是 QML 的 int 属性
    return static_cast<int>(state_of(channel).frames.size());
}

void OpossumBleController::set_strength(int channel, int mode, int value) {
    const bool is_a = (channel == 1);
    const int target = target_strength(is_a, mode, value);
    write_b3(is_a ? target : -1, is_a ? -1 : target);
}

void OpossumBleController::play_wave(int channel, const QStringList& frames) {
    ChannelState& state = mutable_state_of(channel);
    state.frames = frames;
    state.frame_index = 0;
    emit outputWaveChanged();
}

void OpossumBleController::stop_wave(int channel) {
    ChannelState& state = mutable_state_of(channel);
    state.frames.clear();
    state.frame_index = 0;
    emit outputWaveChanged();
}

void OpossumBleController::apply_limits(int limit_a, int limit_b) {
    limit_a_ = std::clamp(limit_a, 0, opossum_frames::kStrengthMax);
    limit_b_ = std::clamp(limit_b, 0, opossum_frames::kStrengthMax);
    LOG_MODULE("OpossumBleController", "apply_limits", LOG_DEBUG, "软上限: A=" << limit_a_ << " B=" << limit_b_);
}

void OpossumBleController::on_controller_connected() {
    // 重连时旧控制器可能补发信号，只认当前控制器的事件
    if (sender() != controller_) {
        return;
    }
    connected_ = true;
    emit stateChanged();
    set_status(QStringLiteral("已连接负鼠，正在发现服务…"));
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
    }
    if (controller_ != nullptr) {
        controller_->discoverServices();
    }
}

void OpossumBleController::on_controller_disconnected() {
    // 重连时旧控制器可能补发信号，只认当前控制器的事件
    if (sender() != controller_) {
        return;
    }
    LOG_MODULE("OpossumBleController", "on_controller_disconnected", LOG_INFO, "负鼠蓝牙连接已断开");
    tick_timer_->stop();
    battery_timer_->stop();
    b2_ack_timer_->stop();
    b2_pending_ack_ = false;
    connected_ = false;
    ready_ = false;
    battery_ = -1;
    strength_a_ = 0;
    strength_b_ = 0;
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
    }
    emit stateChanged();
    emit strengthChanged();
    emit batteryChanged();
    emit outputWaveChanged();
    set_status(QStringLiteral("负鼠蓝牙已断开"));
}

void OpossumBleController::on_service_state_changed(QLowEnergyService::ServiceState state) {
    if (state != QLowEnergyService::RemoteServiceDiscovered) {
        return;
    }
    ensure_characteristics();
}

void OpossumBleController::on_characteristic_changed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() != notify_char_uuid_ || value.isEmpty()) {
        return;
    }
    const quint8 head = static_cast<quint8>(value.at(0));
    if (head == 0xB3 && value.size() >= 3) {
        // B3 回执（含物理按键造成的改变）：更新强度后立刻用 B2 刷新屏幕显示
        strength_a_ = static_cast<quint8>(value.at(1));
        strength_b_ = static_cast<quint8>(value.at(2));
        emit strengthChanged();
        write_b2();
        return;
    }
    if (head == 0xD0) {
        // D0 按键状态集：本版未接入输入侧，只记一次说明
        if (!button_data_logged_) {
            button_data_logged_ = true;
            LOG_MODULE("OpossumBleController", "on_characteristic_changed", LOG_DEBUG,
                "负鼠按键数据暂未接入（D0）: " << value.toHex().toStdString());
        }
        return;
    }
}

void OpossumBleController::on_write_completed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    // 只有 B2 的回执才算数：B3 的写入回执不能顶替 B2 的成功判定
    if (characteristic.uuid() != write_char_uuid_ || !b2_pending_ack_ || value.isEmpty()
        || static_cast<quint8>(value.at(0)) != 0xB2) {
        return;
    }
    b2_pending_ack_ = false;
    b2_ack_timer_->stop();
}

void OpossumBleController::on_service_error(int error) {
    LOG_MODULE("OpossumBleController", "on_service_error", LOG_WARN, "负鼠蓝牙服务错误: " << error);
    if (b2_pending_ack_) {
        fail_b2();
    }
}

void OpossumBleController::on_b2_ack_timeout() {
    if (!b2_pending_ack_) {
        return;
    }
    fail_b2();
}

void OpossumBleController::ensure_characteristics() {
    if (primary_service_ == nullptr) {
        return;
    }
    const auto write_char = primary_service_->characteristic(write_char_uuid_);
    const auto notify_char = primary_service_->characteristic(notify_char_uuid_);
    if (!write_char.isValid() || !notify_char.isValid()) {
        set_status(QStringLiteral("未找到负鼠写入/通知特征"));
        return;
    }
    // 写入 CCCD 打开通知（0x0100 小端）
    const auto cccd = notify_char.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    if (cccd.isValid()) {
        primary_service_->writeDescriptor(cccd, QByteArray::fromHex("0100"));
    }
    ready_ = true;
    emit stateChanged();
    set_status(QStringLiteral("已连接负鼠"));
    // 重连后重发上次绝对强度：设备不会保留断连前的目标值
    if (last_set_a_ >= 0) {
        write_b3(last_set_a_, -1);
    }
    if (last_set_b_ >= 0) {
        write_b3(-1, last_set_b_);
    }
    tick_timer_->start();
}

void OpossumBleController::write_b0() {
    if (primary_service_ == nullptr || !ready_) {
        return;
    }
    const auto write_char = primary_service_->characteristic(write_char_uuid_);
    if (!write_char.isValid()) {
        return;
    }
    const QByteArray data = opossum_frames::build_b0(channel_wave_bytes(1), channel_wave_bytes(2));
    primary_service_->writeCharacteristic(write_char, data, QLowEnergyService::WriteWithoutResponse);
    for (auto& channel : {&channel_a_, &channel_b_}) {
        if (!channel->frames.isEmpty()) {
            channel->frame_index = (channel->frame_index + 1) % channel->frames.size();
        }
    }
    // B0 无回执：每次写入即一次波形推进，事件波形按这个节奏判断是否播完
    emit outputWaveChanged();
}

void OpossumBleController::write_b3(int channel_a, int channel_b) {
    if (primary_service_ == nullptr || !ready_) {
        return;
    }
    const auto write_char = primary_service_->characteristic(write_char_uuid_);
    if (!write_char.isValid()) {
        return;
    }
    const QByteArray data = opossum_frames::build_b3(channel_a, channel_b);
    const auto mode = (write_char.properties() & QLowEnergyCharacteristic::Write)
        ? QLowEnergyService::WriteWithResponse
        : QLowEnergyService::WriteWithoutResponse;
    primary_service_->writeCharacteristic(write_char, data, mode);
    if (channel_a >= 0) {
        last_set_a_ = channel_a;
    }
    if (channel_b >= 0) {
        last_set_b_ = channel_b;
    }
    LOG_MODULE("OpossumBleController", "write_b3", LOG_DEBUG, "强度下发:" << data.toHex().toStdString());
}

void OpossumBleController::write_b2() {
    if (primary_service_ == nullptr || !ready_ || !screen_sync_available_) {
        return;
    }
    const auto write_char = primary_service_->characteristic(write_char_uuid_);
    if (!write_char.isValid()) {
        return;
    }
    const bool with_response = (write_char.properties() & QLowEnergyCharacteristic::Write) != 0;
    // 没有写入回执能力时无法确认 24 字节是否被接受，直接回退到文档标称的 20 字节上限
    if (!b2_short_form_ && !with_response) {
        b2_short_form_ = true;
    }
    const QByteArray data = b2_short_form_ ? opossum_frames::build_b2_short(strength_a_, strength_b_)
                                           : opossum_frames::build_b2(strength_a_, strength_b_);
    if (!with_response) {
        primary_service_->writeCharacteristic(write_char, data, QLowEnergyService::WriteWithoutResponse);
        return;
    }
    b2_pending_ack_ = true;
    b2_ack_timer_->start();
    primary_service_->writeCharacteristic(write_char, data, QLowEnergyService::WriteWithResponse);
}

void OpossumBleController::fail_b2() {
    b2_pending_ack_ = false;
    b2_ack_timer_->stop();
    if (!b2_short_form_) {
        // 24 字节被拒：截断到 20 字节再试一次
        b2_short_form_ = true;
        LOG_MODULE("OpossumBleController", "fail_b2", LOG_WARN, "B2 完整形态写入失败，回退到 20 字节截断形态");
        write_b2();
        return;
    }
    screen_sync_available_ = false;
    emit screenSyncChanged();
    if (!screen_sync_warned_) {
        screen_sync_warned_ = true;
        LOG_MODULE("OpossumBleController", "fail_b2", LOG_WARN,
            "屏幕强度同步不可用（B2 两种形态均写入失败），B0/B3 输出不受影响");
    }
}

void OpossumBleController::subscribe_battery() {
    if (battery_service_ == nullptr) {
        return;
    }
    const auto characteristic = battery_service_->characteristic(kBatteryCharacteristic);
    if (!characteristic.isValid()) {
        LOG_MODULE("OpossumBleController", "subscribe_battery", LOG_WARN, "未找到电量特征");
        return;
    }
    const auto cccd = characteristic.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    if (cccd.isValid() && (characteristic.properties() & QLowEnergyCharacteristic::Notify)) {
        battery_service_->writeDescriptor(cccd, QByteArray::fromHex("0100"));
    }
    read_battery();
    battery_timer_->start();
}

void OpossumBleController::read_battery() {
    if (battery_service_ == nullptr || !connected_) {
        return;
    }
    const auto characteristic = battery_service_->characteristic(kBatteryCharacteristic);
    if (!characteristic.isValid()) {
        return;
    }
    battery_service_->readCharacteristic(characteristic);
}

void OpossumBleController::apply_battery_value(const QByteArray& value) {
    if (value.isEmpty()) {
        return;
    }
    const int percent = static_cast<quint8>(value.at(0));
    if (percent == battery_) {
        return;
    }
    battery_ = percent;
    emit batteryChanged();
}

void OpossumBleController::set_status(const QString& text) {
    if (status_ == text) {
        return;
    }
    status_ = text;
    emit statusChanged();
}

QByteArray OpossumBleController::channel_wave_bytes(int channel) {
    const ChannelState& state = state_of(channel);
    if (state.frames.isEmpty()) {
        return QByteArray(4, '\0');
    }
    const QString& frame = state.frames.at(state.frame_index % state.frames.size());
    const QByteArray bytes = opossum_frames::wave_bytes_from_v3_frame(frame);
    if (bytes.isEmpty()) {
        // 帧不足八字节或非 HEX：跳过该帧并静音，只记一次避免 100ms 级刷屏
        ++invalid_frame_count_;
        if (invalid_frame_count_ == 1) {
            LOG_MODULE("OpossumBleController", "channel_wave_bytes", LOG_WARN,
                "波形帧非法已跳过（负鼠需要 V3 八字节 HEX 帧）: " << frame.toStdString());
        }
        return QByteArray(4, '\0');
    }
    return bytes;
}

const OpossumBleController::ChannelState& OpossumBleController::state_of(int channel) const {
    return (channel == 1) ? channel_a_ : channel_b_;
}

OpossumBleController::ChannelState& OpossumBleController::mutable_state_of(int channel) {
    return (channel == 1) ? channel_a_ : channel_b_;
}

int OpossumBleController::target_strength(bool is_a, int mode, int value) const {
    const int current = is_a ? strength_a_ : strength_b_;
    const int limit = std::clamp(is_a ? limit_a_ : limit_b_, 0, opossum_frames::kStrengthMax);
    int target = current;
    if (mode == 0) {
        target = current - std::max(1, value);
    }
    else if (mode == 1) {
        target = current + std::max(1, value);
    }
    else if (mode == 3) {
        target = current - 1;
    }
    else if (mode == 4) {
        target = current + 1;
    }
    else {
        target = value;
    }
    return std::clamp(target, 0, limit);
}
