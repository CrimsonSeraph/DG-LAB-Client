/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "CivetBleController.h"

#include "DebugLog.h"

#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyDescriptor>
#include <QLowEnergyService>
#include <QTimer>
#include <QVariant>

namespace {

    // 灵猫蓝牙 UUID：服务与写/通知特征与郊狼 V3 相同，另有标准电量服务
    const QBluetoothUuid kPrimaryService(static_cast<quint16>(0x180C));
    const QBluetoothUuid kWriteCharacteristic(static_cast<quint16>(0x150A));
    const QBluetoothUuid kNotifyCharacteristic(static_cast<quint16>(0x150B));
    const QBluetoothUuid kBatteryService(static_cast<quint16>(0x180A));
    const QBluetoothUuid kBatteryCharacteristic(static_cast<quint16>(0x1500));

    const char* kModelCivetV1 = "47L124"; ///< 型号段（广播名 47L124000）

} // namespace

CivetBleController::CivetBleController(QObject* parent)
    : QObject(parent)
    , write_char_uuid_(kWriteCharacteristic)
    , notify_char_uuid_(kNotifyCharacteristic)
    , battery_char_uuid_(kBatteryCharacteristic) {
    write_timer_ = std::make_unique<QTimer>(this);
    write_timer_->setSingleShot(true);
    write_timer_->setInterval(kReportWaitMs);
    connect(write_timer_.get(), &QTimer::timeout, this, &CivetBleController::on_report_timeout);
    command66_timer_ = std::make_unique<QTimer>(this);
    command66_timer_->setSingleShot(true);
    command66_timer_->setInterval(kReportWaitMs);
    connect(command66_timer_.get(), &QTimer::timeout, this, &CivetBleController::on_command66_timeout);
}

CivetBleController::~CivetBleController() {
    disconnect_device();
}

void CivetBleController::initialize() {
    status_ = QStringLiteral("未连接");
    emit statusChanged();
    LOG_MODULE("CivetBleController", "initialize", LOG_INFO, "灵猫驱动已就绪（实验性，输入设备）");
}

void CivetBleController::connect_device(const QBluetoothDeviceInfo& info) {
    if (!info.isValid() || controller_ != nullptr) {
        return;
    }
    device_address_ = info.address().toString();
    device_model_ = QString::fromLatin1(kModelCivetV1);
    device_label_ = QStringLiteral("灵猫");
    controller_ = QLowEnergyController::createCentral(info, this);
    connect(controller_, &QLowEnergyController::connected, this, &CivetBleController::on_controller_connected);
    connect(controller_, &QLowEnergyController::disconnected, this,
        &CivetBleController::on_controller_disconnected);
    connect(controller_, &QLowEnergyController::discoveryFinished, this,
        &CivetBleController::on_services_discovered);
    connect(controller_, &QLowEnergyController::errorOccurred, this, [this](QLowEnergyController::Error) {
        set_status(QStringLiteral("灵猫蓝牙连接失败"));
    });
    set_status(QStringLiteral("正在连接灵猫…（需连按 5 次开机键才可被搜到）"));
    controller_->connectToDevice();
}

void CivetBleController::disconnect_device() {
    if (write_timer_ != nullptr) {
        write_timer_->stop();
    }
    if (command66_timer_ != nullptr) {
        command66_timer_->stop();
    }
    if (controller_ != nullptr) {
        controller_->disconnectFromDevice();
        controller_->deleteLater();
        controller_ = nullptr;
    }
    primary_service_ = nullptr;
    battery_service_ = nullptr;
    write_characteristic_ = QLowEnergyCharacteristic();
    notify_characteristic_ = QLowEnergyCharacteristic();
    battery_characteristic_ = QLowEnergyCharacteristic();
    const bool was_connected = connected_;
    connected_ = false;
    ready_ = false;
    report_acknowledged_ = false;
    report_warned_ = false;
    drop_warned_ = false;
    command66_pending_ = false;
    report_attempts_ = 0;
    command66_attempts_ = 0;
    command66_size_ = 0;
    battery_ = -1;
    pressure_raw_ = 0;
    dropped_reports_ = 0;
    // 设备标识随连接失效：离线设备不再出现在状态文案里
    device_address_.clear();
    device_label_.clear();
    device_model_.clear();
    if (was_connected) {
        emit stateChanged();
        set_status(QStringLiteral("灵猫已断开"));
    }
    else {
        status_ = QStringLiteral("未连接");
        emit statusChanged();
    }
}

void CivetBleController::resetPressureZero() {
    if (primary_service_ == nullptr || !write_characteristic_.isValid()) {
        LOG_MODULE("CivetBleController", "resetPressureZero", LOG_WARN, "灵猫写特征不可用，无法写 66 指令");
        return;
    }
    if (command66_pending_) {
        return;
    }
    command66_attempts_ = 0;
    write_command66();
}

void CivetBleController::on_controller_connected() {
    connected_ = true;
    emit stateChanged();
    set_status(QStringLiteral("灵猫已连接，准备写入 50 指令…"));
    if (controller_ != nullptr) {
        controller_->discoverServices();
    }
}

void CivetBleController::on_controller_disconnected() {
    disconnect_device();
}

void CivetBleController::on_services_discovered() {
    if (controller_ == nullptr) {
        return;
    }
    primary_service_ = controller_->createServiceObject(kPrimaryService, this);
    if (primary_service_ == nullptr) {
        set_status(QStringLiteral("灵猫服务 0x180C 不存在"));
        return;
    }
    connect(primary_service_, &QLowEnergyService::stateChanged, this,
        &CivetBleController::on_service_state_changed);
    connect(primary_service_, &QLowEnergyService::characteristicChanged, this,
        &CivetBleController::on_characteristic_changed);
    connect(primary_service_, &QLowEnergyService::characteristicWritten, this,
        &CivetBleController::on_write_completed);
    connect(primary_service_,
        QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::errorOccurred), this,
        [this](QLowEnergyService::ServiceError error) { on_service_error(static_cast<int>(error)); });
    primary_service_->discoverDetails();
    // 电量服务独立发现：缺失或失败都不影响气压主路径
    battery_service_ = controller_->createServiceObject(kBatteryService, this);
    if (battery_service_ == nullptr) {
        LOG_MODULE("CivetBleController", "on_services_discovered", LOG_WARN, "灵猫电量服务 0x180A 不存在");
        return;
    }
    connect(battery_service_, &QLowEnergyService::stateChanged, this,
        &CivetBleController::on_battery_service_state_changed);
    connect(battery_service_, &QLowEnergyService::characteristicChanged, this,
        &CivetBleController::on_battery_characteristic_changed);
    connect(battery_service_, &QLowEnergyService::characteristicRead, this,
        &CivetBleController::on_characteristic_read);
    battery_service_->discoverDetails();
}

void CivetBleController::on_service_state_changed(QLowEnergyService::ServiceState state) {
    if (state != QLowEnergyService::RemoteServiceDiscovered) {
        return;
    }
    ensure_characteristics();
    if (!ready_) {
        set_status(QStringLiteral("灵猫写特征不可用，无法写入 50 指令"));
        return;
    }
    // 协议要求连上后立即写 50 指令开启气压上报；断连重连同样先写
    write_report_command();
}

void CivetBleController::on_battery_service_state_changed(QLowEnergyService::ServiceState state) {
    if (state != QLowEnergyService::RemoteServiceDiscovered || battery_service_ == nullptr) {
        return;
    }
    battery_characteristic_ = battery_service_->characteristic(battery_char_uuid_);
    if (!battery_characteristic_.isValid()) {
        LOG_MODULE("CivetBleController", "on_battery_service_state_changed", LOG_WARN,
            "灵猫电量特征 0x1500 不可用");
        return;
    }
    const QLowEnergyDescriptor notification = battery_characteristic_.clientCharacteristicConfiguration();
    if (notification.isValid()) {
        battery_service_->writeDescriptor(notification, QByteArray::fromHex("0100"));
    }
    if (battery_characteristic_.properties().testFlag(QLowEnergyCharacteristic::Read)) {
        battery_service_->readCharacteristic(battery_characteristic_);
    }
}

void CivetBleController::on_characteristic_changed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() != notify_char_uuid_) {
        return;
    }
    handle_report(value);
}

void CivetBleController::on_battery_characteristic_changed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() != battery_char_uuid_) {
        return;
    }
    apply_battery(value);
}

void CivetBleController::on_characteristic_read(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() != battery_char_uuid_) {
        return;
    }
    apply_battery(value);
}

void CivetBleController::on_write_completed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() != write_char_uuid_) {
        return;
    }
    // 写入回执只表示 GATT 层写成功：66 指令以回执结束重试，50 指令以首帧 D0 为生效依据
    if (command66_pending_) {
        command66_pending_ = false;
        command66_timer_->stop();
        LOG_MODULE("CivetBleController", "on_write_completed", LOG_INFO,
            "66 指令写入回执（" << command66_size_ << " 字节形态）");
        return;
    }
    LOG_MODULE("CivetBleController", "on_write_completed", LOG_DEBUG,
        "灵猫写入回执: " << value.toHex().toStdString());
}

void CivetBleController::on_service_error(int error) {
    LOG_MODULE("CivetBleController", "on_service_error", LOG_WARN, "灵猫服务错误: " << error);
    if (command66_pending_ && command66_attempts_ < kMaxCommandAttempts) {
        write_command66();
        return;
    }
    if (!report_acknowledged_ && report_attempts_ < kMaxCommandAttempts) {
        write_report_command();
    }
}

void CivetBleController::on_report_timeout() {
    if (report_acknowledged_) {
        return;
    }
    if (report_attempts_ < kMaxCommandAttempts) {
        LOG_MODULE("CivetBleController", "on_report_timeout", LOG_WARN,
            "50 指令未在 " << kReportWaitMs << "ms 内收到气压上报，重试一次");
        write_report_command();
        return;
    }
    if (!report_warned_) {
        report_warned_ = true;
        LOG_MODULE("CivetBleController", "on_report_timeout", LOG_ERROR,
            "50 指令写入 " << report_attempts_ << " 次仍未收到气压上报，设备可能未接受配置");
        emit errorOccurred(QStringLiteral("灵猫未确认 50 指令，气压上报可能不可用"));
    }
    set_status(QStringLiteral("灵猫未确认 50 指令"));
}

void CivetBleController::on_command66_timeout() {
    if (!command66_pending_) {
        return;
    }
    command66_pending_ = false;
    if (command66_attempts_ < kMaxCommandAttempts) {
        LOG_MODULE("CivetBleController", "on_command66_timeout", LOG_WARN,
            "66 指令 " << command66_size_ << " 字节形态未收到写入回执，回退 14 字节");
        write_command66();
        return;
    }
    LOG_MODULE("CivetBleController", "on_command66_timeout", LOG_WARN,
        "66 指令两种形态均未收到回执，气压主路径不受影响");
}

void CivetBleController::ensure_characteristics() {
    if (primary_service_ == nullptr) {
        return;
    }
    write_characteristic_ = primary_service_->characteristic(write_char_uuid_);
    notify_characteristic_ = primary_service_->characteristic(notify_char_uuid_);
    if (notify_characteristic_.isValid()) {
        const QLowEnergyDescriptor notification = notify_characteristic_.clientCharacteristicConfiguration();
        if (notification.isValid()) {
            primary_service_->writeDescriptor(notification, QByteArray::fromHex("0100"));
        }
        else {
            LOG_MODULE("CivetBleController", "ensure_characteristics", LOG_WARN, "灵猫通知特征缺少 CCCD");
        }
    }
    ready_ = write_characteristic_.isValid();
    LOG_MODULE("CivetBleController", "ensure_characteristics", LOG_INFO,
        "灵猫特征就绪: 写 " << write_characteristic_.isValid() << " 通知 " << notify_characteristic_.isValid());
}

void CivetBleController::write_report_command() {
    if (report_acknowledged_ || primary_service_ == nullptr || !write_characteristic_.isValid()) {
        return;
    }
    if (report_attempts_ >= kMaxCommandAttempts) {
        return;
    }
    ++report_attempts_;
    const QByteArray frame = civet_frames::build_report_start();
    const QLowEnergyService::WriteMode mode = write_characteristic_.properties()
            .testFlag(QLowEnergyCharacteristic::Write)
        ? QLowEnergyService::WriteWithResponse
        : QLowEnergyService::WriteWithoutResponse;
    LOG_MODULE("CivetBleController", "write_report_command", LOG_INFO,
        "写入 50 指令（第 " << report_attempts_ << " 次）: " << frame.toHex().toStdString());
    primary_service_->writeCharacteristic(write_characteristic_, frame, mode);
    write_timer_->start();
}

void CivetBleController::write_command66() {
    if (primary_service_ == nullptr || !write_characteristic_.isValid()) {
        return;
    }
    if (command66_attempts_ >= kMaxCommandAttempts) {
        return;
    }
    ++command66_attempts_;
    // 先按文档标称的 13 字节发送，失败回退 14 字节（与负鼠 B2 同策略）
    const QByteArray frame = command66_attempts_ == 1
        ? civet_frames::build_command66(civet_frames::kScreenRotateKeep, civet_frames::kPressureResetWord)
        : civet_frames::build_command66_wide(civet_frames::kScreenRotateKeep, civet_frames::kPressureResetWord);
    command66_size_ = static_cast<int>(frame.size());
    command66_pending_ = true;
    const QLowEnergyService::WriteMode mode = write_characteristic_.properties()
            .testFlag(QLowEnergyCharacteristic::Write)
        ? QLowEnergyService::WriteWithResponse
        : QLowEnergyService::WriteWithoutResponse;
    LOG_MODULE("CivetBleController", "write_command66", LOG_INFO,
        "写入 66 指令（第 " << command66_attempts_ << " 次，" << command66_size_ << " 字节）: "
                            << frame.toHex().toStdString());
    primary_service_->writeCharacteristic(write_characteristic_, frame, mode);
    command66_timer_->start();
}

void CivetBleController::handle_report(const QByteArray& data) {
    const civet_frames::PressureReport report = civet_frames::parse_report(data);
    if (!report.valid) {
        ++dropped_reports_;
        if (!drop_warned_) {
            drop_warned_ = true;
            LOG_MODULE("CivetBleController", "handle_report", LOG_WARN,
                "收到不可解析的气压帧（长度 " << data.size() << "，需 >= " << civet_frames::kMinReportSize
                    << " 字节且以 0xD0 开头），丢弃该帧，后续同类帧只计数");
        }
        return;
    }
    pressure_raw_ = report.pressure_raw;
    if (!report_acknowledged_) {
        report_acknowledged_ = true;
        if (write_timer_ != nullptr) {
            write_timer_->stop();
        }
        LOG_MODULE("CivetBleController", "handle_report", LOG_INFO,
            "50 指令已生效（首帧 D0，长度 " << report.size << "，气压 " << report.pressure_raw
                << " 即 " << civet_frames::pressure_kpa(report.pressure_raw) << " kPa）");
        set_status(QStringLiteral("灵猫已连接（实验性输入设备）"));
    }
    emit pressureReceived(report.pressure_raw);
}

void CivetBleController::apply_battery(const QByteArray& value) {
    const int percent = civet_frames::battery_percent(value);
    if (percent < 0 || percent == battery_) {
        return;
    }
    battery_ = percent;
    LOG_MODULE("CivetBleController", "apply_battery", LOG_INFO, "灵猫电量 " << percent << "%");
    emit batteryReceived(percent);
}

void CivetBleController::set_status(const QString& text) {
    if (status_ == text) {
        return;
    }
    status_ = text;
    emit statusChanged();
}
