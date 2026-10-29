/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DeviceValueBridge.h"

#include "AppConfig.h"
#include "CivetBleController.h"
#include "DebugLog.h"
#include "EventRuntime.h"
#include "ModuleManager.h"
#include "PawPrintsBleController.h"

namespace {

    constexpr const char* kUserConfigName = "user"; ///< 写入的配置层（映射与迁移都落 user.json）
    constexpr const char* kPluginsKeyPrefix = "app.plugins."; ///< 模块映射键前缀
    constexpr const char* kMigratedMarker = "module_migrated_to"; ///< 旧映射迁移标记（写在旧模块名下）

    // 读取配置子树（缺失或非对象返回空对象）
    nlohmann::json read_subtree(const std::string& key) {
        const nlohmann::json value = AppConfig::instance().get_value<nlohmann::json>(key, nlohmann::json::object());
        return value.is_object() ? value : nlohmann::json::object();
    }

    // 写入配置子树到 user 层（配置系统未初始化时跳过）
    void write_subtree(const std::string& key, const nlohmann::json& value) {
        AppConfig& config = AppConfig::instance();
        if (!config.is_initialized()) {
            return;
        }
        config.set_value_with_name<nlohmann::json>(key, value, kUserConfigName);
    }

} // namespace

DeviceValueBridge::DeviceValueBridge(DeviceKind kind, std::string module_name, std::string display_name,
    QObject* driver, QObject* parent)
    : QObject(parent)
    , kind_(kind)
    , module_name_(std::move(module_name))
    , display_name_(std::move(display_name))
    , driver_(driver) {
}

DeviceValueBridge::~DeviceValueBridge() {
    // 断开路径没走到时兜底注销，避免进程活着的期间残留一个永远不更新的模块
    shutdown();
}

void DeviceValueBridge::initialize() {
    d0_values_ = AppConfig::instance().get_value<bool>("app.devices.paw.d0_values", false);
    civet_epsilon_ = AppConfig::instance().get_value<int>("app.devices.civet.pressure_epsilon",
        kCivetDefaultEpsilon);
    // 迁移必须在注册内置声明之前完成：随后的一次 reload 才会读到迁移后的映射
    migrate_legacy_mapping();
    if (kind_ == DeviceKind::PawPrints) {
        initialize_paw();
    }
    else {
        initialize_civet();
    }
}

void DeviceValueBridge::shutdown() {
    if (shut_down_) {
        return;
    }
    shut_down_ = true;
    // 顺序固定：先注销数值模块（模块列表与规则引用里不再出现该实例），
    // 再清掉该实例的内置事件声明，最后由事件运行时丢弃它的运行期状态
    ModuleManager::instance().unregister_module(module_name_);
    if (event_runtime_ != nullptr) {
        event_runtime_->unregister_builtin_events(module_name_);
    }
    LOG_MODULE("DeviceValueBridge", "shutdown", LOG_INFO,
        "实例数值与事件声明已注销: " << module_name_ << "（" << display_name_ << "）");
}

void DeviceValueBridge::initialize_paw() {
    ModuleManager& modules = ModuleManager::instance();
    if (!modules.register_module_values(module_name_, paw_values::build(), {})) {
        LOG_MODULE("DeviceValueBridge", "initialize_paw", LOG_WARN,
            "爪印数值注册失败: " << module_name_);
        return;
    }
    // 内置声明先接进引擎：注册会重载事件引擎并清空前值基线，
    // 之后的基线写入才会成为引擎眼里的前值，设备第一次按键不会因为「首个观测值只登记前值」被吞掉
    register_builtin_events();
    // 初始状态写入：首次写入只登记前值，不触发任何事件
    set_value("button", 0);
    PawPrintsBleController* paw = paw_driver();
    if (paw == nullptr) {
        LOG_MODULE("DeviceValueBridge", "initialize_paw", LOG_WARN,
            "爪印驱动未接入，仅注册数值与事件声明: " << module_name_);
        return;
    }
    connect(paw, &PawPrintsBleController::deviceInfoReceived, this, &DeviceValueBridge::on_device_info);
    connect(paw, &PawPrintsBleController::triggerStarted, this, &DeviceValueBridge::on_trigger_started);
    connect(paw, &PawPrintsBleController::triggerCancelled, this, &DeviceValueBridge::on_trigger_cancelled);
    connect(paw, &PawPrintsBleController::paramChanged, this, &DeviceValueBridge::on_param_changed);
    connect(paw, &PawPrintsBleController::physicalDataReceived, this, &DeviceValueBridge::on_physical_data);
    LOG_MODULE("DeviceValueBridge", "initialize_paw", LOG_INFO,
        "爪印数值桥接完成: " << module_name_ << "（" << display_name_ << "）数值 " << paw_values::kValueCount
                            << " 个，D0 物理数据落值 " << (d0_values_ ? "开启" : "关闭"));
}

void DeviceValueBridge::initialize_civet() {
    if (!ModuleManager::instance().register_module_values(module_name_, civet_values::build(), {})) {
        LOG_MODULE("DeviceValueBridge", "initialize_civet", LOG_WARN,
            "灵猫数值注册失败: " << module_name_);
        return;
    }
    // 内置声明先接进引擎（注册会重载并清空前值基线），再写基线
    register_builtin_events();
    // 初始基线写入：首帧气压按同一阈值与此基线比较
    set_civet_value("raw", 0);
    set_civet_value("pressure", 0);
    CivetBleController* civet = civet_driver();
    if (civet == nullptr) {
        LOG_MODULE("DeviceValueBridge", "initialize_civet", LOG_WARN,
            "灵猫驱动未接入，仅注册数值与事件声明: " << module_name_);
        return;
    }
    connect(civet, &CivetBleController::pressureReceived, this, &DeviceValueBridge::on_civet_pressure);
    connect(civet, &CivetBleController::batteryReceived, this, &DeviceValueBridge::on_civet_battery);
    LOG_MODULE("DeviceValueBridge", "initialize_civet", LOG_INFO,
        "灵猫数值桥接完成: " << module_name_ << "（" << display_name_ << "）数值 " << civet_values::kValueCount
                            << " 个，气压落值阈值 " << civet_epsilon_
                            << "（0.01 kPa），每次上报推进 pressure / raw");
}

void DeviceValueBridge::register_builtin_events() {
    if (event_runtime_ == nullptr) {
        return;
    }
    // 模块标识就是实例名：两台爪印各自一份声明，彼此不覆盖
    event_runtime_->register_builtin_events(module_name_,
        kind_ == DeviceKind::PawPrints ? builtin_events() : civet_builtin_events());
}

void DeviceValueBridge::migrate_legacy_mapping() {
    const std::string legacy = kind_ == DeviceKind::PawPrints ? kModuleName : kCivetModuleName;
    if (module_name_ == legacy || !AppConfig::instance().is_initialized()) {
        return;
    }
    const std::string legacy_key = std::string(kPluginsKeyPrefix) + legacy;
    const nlohmann::json legacy_mapping = read_subtree(legacy_key);
    if (legacy_mapping.empty()) {
        return;
    }
    // 一次性迁移：旧模块名下的映射只搬到第一台连上的实例，之后连上的设备不再抢它
    const std::string marker = AppConfig::instance().get_value<std::string>(
        legacy_key + "." + kMigratedMarker, std::string());
    if (!marker.empty()) {
        LOG_MODULE("DeviceValueBridge", "migrate_legacy_mapping", LOG_DEBUG,
            "旧模块映射已迁移至 " << marker << "，本次不再迁移: " << module_name_);
        return;
    }
    const std::string instance_key = std::string(kPluginsKeyPrefix) + module_name_;
    if (!read_subtree(instance_key).empty()) {
        // 实例自己已有映射：以实例映射为准，旧映射保持原样（不动用户配置）
        return;
    }
    nlohmann::json migrated = nlohmann::json::object();
    for (auto it = legacy_mapping.begin(); it != legacy_mapping.end(); ++it) {
        if (it.key() == kMigratedMarker) {
            continue;
        }
        migrated[it.key()] = it.value();
    }
    if (migrated.empty()) {
        return;
    }
    write_subtree(instance_key, migrated);
    AppConfig::instance().set_value_with_name<std::string>(legacy_key + "." + kMigratedMarker,
        module_name_, kUserConfigName);
    AppConfig::instance().save_all();
    LOG_MODULE("DeviceValueBridge", "migrate_legacy_mapping", LOG_INFO,
        "旧模块映射已迁移: " << legacy_key << " → " << instance_key
                            << "（旧键保留并在 " << kMigratedMarker << " 标记去向，可手工删除）");
}

PawPrintsBleController* DeviceValueBridge::paw_driver() const {
    return qobject_cast<PawPrintsBleController*>(driver_);
}

CivetBleController* DeviceValueBridge::civet_driver() const {
    return qobject_cast<CivetBleController*>(driver_);
}

void DeviceValueBridge::on_device_info(int indicator_color, int device_type, int battery) {
    if (battery >= 0) {
        set_value("battery", battery);
    }
    LOG_MODULE("DeviceValueBridge", "on_device_info", LOG_DEBUG,
        "爪印设备信息: 模块 " << module_name_ << " 指示灯 " << indicator_color << " 类型 " << device_type
                            << " 电量 " << battery);
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
                "D0 物理数据未写入数值（app.devices.paw.d0_values 为 false）: " << module_name_);
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
    ModuleManager::instance().set_value(module_name_, value_id, value);
}

void DeviceValueBridge::on_civet_pressure(int pressure_raw) {
    if (!civet_values::pressure_changed(civet_last_pressure_, pressure_raw, civet_epsilon_)) {
        // 气压 10Hz 上报：未达阈值不写值，避免以 10Hz 触发规则求值
        ++civet_suppressed_;
        if (!civet_suppressed_logged_) {
            civet_suppressed_logged_ = true;
            LOG_MODULE("DeviceValueBridge", "on_civet_pressure", LOG_DEBUG,
                "灵猫气压变化未达阈值 " << civet_epsilon_ << "（0.01 kPa），本次上报丢弃并计数: "
                                        << module_name_);
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
    ModuleManager::instance().set_value(module_name_, value_id, value);
}
