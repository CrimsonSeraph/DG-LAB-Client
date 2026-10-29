/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DeviceValueBridge.h"

#include "AppConfig.h"
#include "CivetBleController.h"
#include "DebugLog.h"
#include "ModuleManager.h"
#include "PawPrintsBleController.h"

DeviceValueBridge::DeviceValueBridge(PawPrintsBleController* paw, CivetBleController* civet,
    QObject* parent)
    : QObject(parent)
    , paw_(paw)
    , civet_(civet) {
}

DeviceValueBridge::~DeviceValueBridge() = default;

void DeviceValueBridge::initialize() {
    d0_values_ = AppConfig::instance().get_value<bool>("app.devices.paw.d0_values", false);
    civet_epsilon_ = AppConfig::instance().get_value<int>("app.devices.civet.pressure_epsilon",
        kCivetDefaultEpsilon);
    initialize_paw();
    initialize_civet();
}

void DeviceValueBridge::initialize_paw() {
    ModuleManager& modules = ModuleManager::instance();
    if (!modules.register_module_values(kModuleName, paw_values::build(), {})) {
        LOG_MODULE("DeviceValueBridge", "initialize_paw", LOG_WARN, "paw 数值注册失败");
        return;
    }
    // 初始状态写入：首次写入只登记前值，不触发任何事件
    set_value("button", 0);
    if (paw_ == nullptr) {
        LOG_MODULE("DeviceValueBridge", "initialize_paw", LOG_WARN, "爪印驱动未接入，仅注册数值");
        return;
    }
    connect(paw_, &PawPrintsBleController::deviceInfoReceived, this, &DeviceValueBridge::on_device_info);
    connect(paw_, &PawPrintsBleController::triggerStarted, this, &DeviceValueBridge::on_trigger_started);
    connect(paw_, &PawPrintsBleController::triggerCancelled, this, &DeviceValueBridge::on_trigger_cancelled);
    connect(paw_, &PawPrintsBleController::paramChanged, this, &DeviceValueBridge::on_param_changed);
    connect(paw_, &PawPrintsBleController::physicalDataReceived, this, &DeviceValueBridge::on_physical_data);
    LOG_MODULE("DeviceValueBridge", "initialize_paw", LOG_INFO,
        "paw 数值桥接完成，数值 " << paw_values::kValueCount << " 个，D0 物理数据落值 "
            << (d0_values_ ? "开启" : "关闭"));
}

void DeviceValueBridge::initialize_civet() {
    if (!ModuleManager::instance().register_module_values(kCivetModuleName, civet_values::build(), {})) {
        LOG_MODULE("DeviceValueBridge", "initialize_civet", LOG_WARN, "civet 数值注册失败");
        return;
    }
    // 初始基线写入：首帧气压按同一阈值与此基线比较
    set_civet_value("raw", 0);
    set_civet_value("pressure", 0);
    if (civet_ == nullptr) {
        LOG_MODULE("DeviceValueBridge", "initialize_civet", LOG_WARN, "灵猫驱动未接入，仅注册数值");
        return;
    }
    connect(civet_, &CivetBleController::pressureReceived, this, &DeviceValueBridge::on_civet_pressure);
    connect(civet_, &CivetBleController::batteryReceived, this, &DeviceValueBridge::on_civet_battery);
    LOG_MODULE("DeviceValueBridge", "initialize_civet", LOG_INFO,
        "civet 数值桥接完成，数值 " << civet_values::kValueCount << " 个，气压落值阈值 " << civet_epsilon_
                                    << "（0.01 kPa），每次上报推进 pressure / raw");
}

void DeviceValueBridge::on_device_info(int indicator_color, int device_type, int battery) {
    if (battery >= 0) {
        set_value("battery", battery);
    }
    LOG_MODULE("DeviceValueBridge", "on_device_info", LOG_DEBUG,
        "爪印设备信息: 指示灯 " << indicator_color << " 类型 " << device_type << " 电量 " << battery);
}

void DeviceValueBridge::on_trigger_started(int event_id, int param) {
    if (event_id >= 0) {
        set_value("event_id", event_id);
    }
    if (param >= 0) {
        set_value("param", param);
    }
    set_value("button", 1);
}

void DeviceValueBridge::on_trigger_cancelled(int event_id) {
    if (event_id >= 0) {
        set_value("event_id", event_id);
    }
    set_value("button", 0);
}

void DeviceValueBridge::on_param_changed(int event_id, int param) {
    if (event_id >= 0) {
        set_value("event_id", event_id);
    }
    if (param >= 0) {
        set_value("param", param);
    }
}

void DeviceValueBridge::on_physical_data(int pressed, int accel, int angle_x, int angle_y, int angle_z,
    int voltage) {
    if (!d0_values_) {
        // D0 为 10Hz 全量数据：默认不落值，避免 10Hz 触发规则求值
        if (!d0_skipped_logged_) {
            d0_skipped_logged_ = true;
            LOG_MODULE("DeviceValueBridge", "on_physical_data", LOG_DEBUG,
                "D0 物理数据未写入数值（app.devices.paw.d0_values 为 false）");
        }
        return;
    }
    set_value("button", pressed != 0 ? 1 : 0);
    set_value("accel", accel);
    set_value("angle_x", angle_x);
    set_value("angle_y", angle_y);
    set_value("angle_z", angle_z);
    set_value("voltage", voltage);
}

void DeviceValueBridge::set_value(const char* value_id, int value) {
    ModuleManager::instance().set_value(kModuleName, value_id, value);
}

void DeviceValueBridge::on_civet_pressure(int pressure_raw) {
    if (!civet_values::pressure_changed(civet_last_pressure_, pressure_raw, civet_epsilon_)) {
        // 气压 10Hz 上报：未达阈值不写值，避免以 10Hz 触发规则求值
        ++civet_suppressed_;
        if (!civet_suppressed_logged_) {
            civet_suppressed_logged_ = true;
            LOG_MODULE("DeviceValueBridge", "on_civet_pressure", LOG_DEBUG,
                "灵猫气压变化未达阈值 " << civet_epsilon_ << "（0.01 kPa），本次上报丢弃并计数");
        }
        return;
    }
    civet_last_pressure_ = pressure_raw;
    set_civet_value("raw", pressure_raw);
    set_civet_value("pressure", pressure_raw);
}

void DeviceValueBridge::on_civet_battery(int percent) {
    set_civet_value("battery", percent);
}

void DeviceValueBridge::set_civet_value(const char* value_id, int value) {
    ModuleManager::instance().set_value(kCivetModuleName, value_id, value);
}
