/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "PawPrintsBleController.h"

#include "DebugLog.h"

#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyDescriptor>
#include <QLowEnergyService>
#include <QTimer>
#include <QVariant>

#include <algorithm>

namespace {

    // 爪印无线按钮传感器蓝牙 UUID（与郊狼 V3 相同的 16 位短 UUID）
    const QBluetoothUuid kPrimaryService(static_cast<quint16>(0x180C));
    const QBluetoothUuid kWriteCharacteristic(static_cast<quint16>(0x150A));
    const QBluetoothUuid kNotifyCharacteristic(static_cast<quint16>(0x150B));

    const char* kModelPawV11 = "47L120"; ///< 型号段（广播名 47L120300）

} // namespace

PawPrintsBleController::PawPrintsBleController(QObject* parent)
    : QObject(parent)
    , write_char_uuid_(kWriteCharacteristic)
    , notify_char_uuid_(kNotifyCharacteristic) {
    write_timer_ = std::make_unique<QTimer>(this);
    write_timer_->setSingleShot(true);
    write_timer_->setInterval(kWriteRetryMs);
    connect(write_timer_.get(), &QTimer::timeout, this, &PawPrintsBleController::on_write_timeout);
}

PawPrintsBleController::~PawPrintsBleController() {
    disconnect_device();
}

void PawPrintsBleController::initialize() {
    status_ = QStringLiteral("未连接");
    emit statusChanged();
    LOG_MODULE("PawPrintsBleController", "initialize", LOG_INFO, "爪印驱动已就绪（实验性，输入设备）");
}

void PawPrintsBleController::connect_device(const QBluetoothDeviceInfo& info) {
    if (!info.isValid() || controller_ != nullptr) {
        return;
    }
    device_address_ = info.address().toString();
    device_model_ = QString::fromLatin1(kModelPawV11);
    device_label_ = QStringLiteral("爪印");
    controller_ = QLowEnergyController::createCentral(info, this);
    connect(controller_, &QLowEnergyController::connected, this, &PawPrintsBleController::on_controller_connected);
    connect(controller_, &QLowEnergyController::disconnected, this,
        &PawPrintsBleController::on_controller_disconnected);
    connect(controller_, &QLowEnergyController::discoveryFinished, this, [this]() {
        if (controller_ == nullptr) {
            return;
        }
        primary_service_ = controller_->createServiceObject(kPrimaryService, this);
        if (primary_service_ == nullptr) {
            set_status(QStringLiteral("爪印服务 0x180C 不存在"));
            return;
        }
        connect(primary_service_, &QLowEnergyService::stateChanged, this,
            &PawPrintsBleController::on_service_state_changed);
        connect(primary_service_, &QLowEnergyService::characteristicChanged, this,
            &PawPrintsBleController::on_characteristic_changed);
        connect(primary_service_, &QLowEnergyService::characteristicWritten, this,
            &PawPrintsBleController::on_write_completed);
        connect(primary_service_,
            QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::errorOccurred), this,
            [this](QLowEnergyService::ServiceError error) { on_service_error(static_cast<int>(error)); });
        primary_service_->discoverDetails();
    });
    connect(controller_, &QLowEnergyController::errorOccurred, this, [this](QLowEnergyController::Error) {
        set_status(QStringLiteral("爪印蓝牙连接失败"));
    });
    set_status(QStringLiteral("正在连接爪印…"));
    controller_->connectToDevice();
}

void PawPrintsBleController::disconnect_device() {
    if (write_timer_ != nullptr) {
        write_timer_->stop();
    }
    if (controller_ != nullptr) {
        controller_->disconnectFromDevice();
        controller_->deleteLater();
        controller_ = nullptr;
    }
    primary_service_ = nullptr;
    write_characteristic_ = QLowEnergyCharacteristic();
    notify_characteristic_ = QLowEnergyCharacteristic();
    const bool was_connected = connected_;
    connected_ = false;
    ready_ = false;
    config_acknowledged_ = false;
    config_warned_ = false;
    write_attempts_ = 0;
    battery_ = -1;
    indicator_color_ = 0;
    angle_thresholds_.clear();
    // 设备标识随连接失效：离线设备不再出现在状态文案里
    device_address_.clear();
    device_label_.clear();
    device_model_.clear();
    if (was_connected) {
        emit stateChanged();
        emit batteryChanged();
        set_status(QStringLiteral("爪印已断开"));
    }
    else {
        status_ = QStringLiteral("未连接");
        emit statusChanged();
    }
}

void PawPrintsBleController::on_controller_connected() {
    connected_ = true;
    emit stateChanged();
    set_status(QStringLiteral("爪印已连接，准备写入 50 指令…"));
    if (controller_ != nullptr) {
        controller_->discoverServices();
    }
}

void PawPrintsBleController::on_controller_disconnected() {
    disconnect_device();
}

void PawPrintsBleController::on_service_state_changed(QLowEnergyService::ServiceState state) {
    if (state != QLowEnergyService::RemoteServiceDiscovered) {
        return;
    }
    ensure_characteristics();
    if (!ready_) {
        set_status(QStringLiteral("爪印写特征不可用，无法写入 50 指令"));
        return;
    }
    // 协议要求连上后立即写 50 指令，否则设备可能断开；重连同样先写
    write_config();
}

void PawPrintsBleController::on_characteristic_changed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() != notify_char_uuid_) {
        return;
    }
    handle_message(paw_frames::parse_message(value));
}

void PawPrintsBleController::on_write_completed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() != write_char_uuid_) {
        return;
    }
    // 写入回执只表示 GATT 层写成功，50 指令是否被接受以 51 消息为准
    LOG_MODULE("PawPrintsBleController", "on_write_completed", LOG_DEBUG,
        "爪印写入回执: " << value.toHex().toStdString());
}

void PawPrintsBleController::on_service_error(int error) {
    LOG_MODULE("PawPrintsBleController", "on_service_error", LOG_WARN, "爪印服务错误: " << error);
    if (!config_acknowledged_ && write_attempts_ < kMaxWriteAttempts) {
        write_config();
    }
}

void PawPrintsBleController::on_write_timeout() {
    if (config_acknowledged_) {
        return;
    }
    if (write_attempts_ < kMaxWriteAttempts) {
        LOG_MODULE("PawPrintsBleController", "on_write_timeout", LOG_WARN,
            "50 指令未在 " << kWriteRetryMs << "ms 内收到 51 回执，重试一次");
        write_config();
        return;
    }
    if (!config_warned_) {
        config_warned_ = true;
        LOG_MODULE("PawPrintsBleController", "on_write_timeout", LOG_ERROR,
            "50 指令写入 " << write_attempts_ << " 次仍未收到 51 回执，设备可能未接受配置");
        emit errorOccurred(QStringLiteral("爪印未确认 50 指令，按键触发可能不可用"));
    }
    set_status(QStringLiteral("爪印未确认 50 指令"));
}

void PawPrintsBleController::ensure_characteristics() {
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
            LOG_MODULE("PawPrintsBleController", "ensure_characteristics", LOG_WARN, "爪印通知特征缺少 CCCD");
        }
    }
    ready_ = write_characteristic_.isValid();
    LOG_MODULE("PawPrintsBleController", "ensure_characteristics", LOG_INFO,
        "爪印特征就绪: 写 " << write_characteristic_.isValid() << " 通知 " << notify_characteristic_.isValid());
}

void PawPrintsBleController::write_config() {
    if (config_acknowledged_ || primary_service_ == nullptr || !write_characteristic_.isValid()) {
        return;
    }
    if (write_attempts_ >= kMaxWriteAttempts) {
        return;
    }
    ++write_attempts_;
    const QByteArray frame = paw_frames::build_config(paw_frames::kDefaultIndicatorColor,
        paw_frames::trigger_settings_button(paw_frames::ButtonTriggerSettings()));
    if (frame.isEmpty()) {
        LOG_MODULE("PawPrintsBleController", "write_config", LOG_ERROR, "50 指令构造失败，放弃写入");
        return;
    }
    const QLowEnergyService::WriteMode mode = write_characteristic_.properties()
            .testFlag(QLowEnergyCharacteristic::Write)
        ? QLowEnergyService::WriteWithResponse
        : QLowEnergyService::WriteWithoutResponse;
    LOG_MODULE("PawPrintsBleController", "write_config", LOG_INFO,
        "写入 50 指令（第 " << write_attempts_ << " 次）: " << frame.toHex().toStdString());
    primary_service_->writeCharacteristic(write_characteristic_, frame, mode);
    write_timer_->start();
}

void PawPrintsBleController::handle_message(const paw_frames::Message& message) {
    switch (message.kind) {
    case paw_frames::MessageKind::DeviceInfo: {
        const bool ack_first_time = !config_acknowledged_;
        config_acknowledged_ = true;
        write_timer_->stop();
        indicator_color_ = message.indicator_color;
        const bool battery_changed = battery_ != message.battery;
        battery_ = message.battery;
        emit deviceInfoReceived(message.indicator_color, message.device_type, message.battery);
        if (battery_changed) {
            emit batteryChanged();
        }
        if (ack_first_time) {
            LOG_MODULE("PawPrintsBleController", "handle_message", LOG_INFO,
                "50 指令已被接受，设备类型 " << message.device_type << " 电量 " << message.battery << "%");
            set_status(QStringLiteral("爪印已连接（实验性输入设备）"));
        }
        return;
    }
    case paw_frames::MessageKind::TriggerStart:
        emit triggerStarted(message.event_id, message.param);
        return;
    case paw_frames::MessageKind::TriggerCancel:
        emit triggerCancelled(message.event_id);
        return;
    case paw_frames::MessageKind::ParamChange:
        emit paramChanged(message.event_id, message.param);
        return;
    case paw_frames::MessageKind::Physical:
        emit physicalDataReceived(message.pressed, message.accel, message.angle_x, message.angle_y,
            message.angle_z, message.voltage);
        return;
    case paw_frames::MessageKind::AngleSelfTest:
        angle_thresholds_.clear();
        for (int axis = 0; axis < 3; ++axis) {
            angle_thresholds_.append(paw_frames::halve_threshold(message.threshold_low[axis]));
            angle_thresholds_.append(paw_frames::halve_threshold(message.threshold_high[axis]));
        }
        // 取整规则待上游确认：只做展示与数值上报，不做阈值判定
        LOG_MODULE("PawPrintsBleController", "handle_message", LOG_INFO,
            "爪印自检角度阈值（已 /2）: " << message.threshold_low[0] << "~" << message.threshold_high[0]
                << " / " << message.threshold_low[1] << "~" << message.threshold_high[1]
                << " / " << message.threshold_low[2] << "~" << message.threshold_high[2]);
        emit angleThresholdsChanged();
        return;
    case paw_frames::MessageKind::Unknown:
        LOG_MODULE("PawPrintsBleController", "handle_message", LOG_DEBUG, "爪印未识别消息，已丢弃");
        return;
    }
}

void PawPrintsBleController::set_status(const QString& text) {
    if (status_ == text) {
        return;
    }
    status_ = text;
    emit statusChanged();
}
