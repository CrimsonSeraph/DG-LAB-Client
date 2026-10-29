/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "EventDefinition.h"
#include "ModuleValue.h"

#include <QObject>

#include <string>
#include <vector>

class CivetBleController;
class EventRuntime;
class PawPrintsBleController;

// paw_values - 爪印在数值模块 `paw` 下的数值定义（驱动、桥接与单元测试共用）
namespace paw_values {

    /// @brief 单个数值的定义（id / 展示名 / 钳制范围）
    struct ValueSpec {
        const char* id;   ///< 数值 ID（规则与事件引用）
        const char* name; ///< 数值中文名
        int min;          ///< 下限
        int max;          ///< 上限
    };

    inline constexpr ValueSpec kValues[] = {
        {"button", "按下状态", 0, 1},
        {"param", "触发参数", 0, 255},
        {"event_id", "触发事件 ID", 0, 24},
        {"accel", "加速度", 0, 255},
        {"angle_x", "X 轴角度", -128, 127},
        {"angle_y", "Y 轴角度", -128, 127},
        {"angle_z", "Z 轴角度", -128, 127},
        {"voltage", "外部电压", 0, 255},
        {"battery", "电量", 0, 100},
    };
    constexpr int kValueCount = static_cast<int>(sizeof(kValues) / sizeof(kValues[0])); ///< 数值个数

    /// @brief 按 id 查数值定义
    /// @param id 数值 ID
    /// @return 定义指针，未登记返回 nullptr
    inline const ValueSpec* find(const std::string& id) {
        for (int index = 0; index < kValueCount; ++index) {
            if (id == kValues[index].id) {
                return &kValues[index];
            }
        }
        return nullptr;
    }

    /// @brief 把数值钳制到登记范围
    /// @param id 数值 ID
    /// @param value 原始数值
    /// @return 钳制后的数值；未登记 id 原样返回
    inline int clamp(const std::string& id, int value) {
        const ValueSpec* spec = find(id);
        if (spec == nullptr) {
            return value;
        }
        if (value < spec->min) {
            return spec->min;
        }
        if (value > spec->max) {
            return spec->max;
        }
        return value;
    }

    /// @brief 构造注册用的数值列表（范围与 kValues 一致，写入时由 ModuleValue 再钳制一次）
    /// @return 数值列表
    inline std::vector<ModuleValue> build() {
        std::vector<ModuleValue> values;
        values.reserve(kValueCount);
        for (int index = 0; index < kValueCount; ++index) {
            const ValueSpec& spec = kValues[index];
            values.emplace_back(spec.id, spec.name, QueryPeriod::SECOND, spec.id, spec.min, spec.max);
        }
        return values;
    }

} // namespace paw_values

// civet_values - 灵猫在数值模块 `civet` 下的数值定义（驱动、桥接与单元测试共用）
namespace civet_values {

    /// @brief 单个数值的定义（id / 展示名 / 钳制范围）
    struct ValueSpec {
        const char* id;   ///< 数值 ID（规则与事件引用）
        const char* name; ///< 数值中文名
        int min;          ///< 下限
        int max;          ///< 上限
    };

    // 量程与温补待上游确认：气压只做线性换算并按 int16 原样登记，不设量程假设
    inline constexpr ValueSpec kValues[] = {
        {"pressure", "气压", -32768, 32767},
        {"battery", "电量", 0, 100},
        {"raw", "气压原始值", -32768, 32767},
    };
    constexpr int kValueCount = static_cast<int>(sizeof(kValues) / sizeof(kValues[0])); ///< 数值个数

    /// @brief 按 id 查数值定义
    /// @param id 数值 ID
    /// @return 定义指针，未登记返回 nullptr
    inline const ValueSpec* find(const std::string& id) {
        for (int index = 0; index < kValueCount; ++index) {
            if (id == kValues[index].id) {
                return &kValues[index];
            }
        }
        return nullptr;
    }

    /// @brief 把数值钳制到登记范围
    /// @param id 数值 ID
    /// @param value 原始数值
    /// @return 钳制后的数值；未登记 id 原样返回
    inline int clamp(const std::string& id, int value) {
        const ValueSpec* spec = find(id);
        if (spec == nullptr) {
            return value;
        }
        if (value < spec->min) {
            return spec->min;
        }
        if (value > spec->max) {
            return spec->max;
        }
        return value;
    }

    /// @brief 构造注册用的数值列表（范围与 kValues 一致，写入时由 ModuleValue 再钳制一次）
    /// @return 数值列表
    inline std::vector<ModuleValue> build() {
        std::vector<ModuleValue> values;
        values.reserve(kValueCount);
        for (int index = 0; index < kValueCount; ++index) {
            const ValueSpec& spec = kValues[index];
            values.emplace_back(spec.id, spec.name, QueryPeriod::SECOND, spec.id, spec.min, spec.max);
        }
        return values;
    }

    /// @brief 气压变化是否达到落值阈值（阈值单位与数值一致，均为 0.01 kPa）
    /// @param previous 上一次落值
    /// @param current 本次上报值
    /// @param epsilon 阈值；小于等于 0 时退化为「与原值不同即落值」
    /// @return true 表示变化达到阈值
    inline bool pressure_changed(int previous, int current, int epsilon) {
        if (epsilon <= 0) {
            return previous != current;
        }
        const int delta = current > previous ? current - previous : previous - current;
        return delta >= epsilon;
    }

} // namespace civet_values

// DeviceValueBridge - 单台输入设备驱动到数值模块的桥接（爪印 → paw_<后四位>，灵猫 → civet_<后四位>）
// 一台设备一份桥接：模块名由链路管理器按 MAC 分配（paw_430C 之类），多台同型号不会互相覆盖。
// 把驱动的通知信号写成数值（全部经 ModuleManager::set_value，自动钳制并触发 value_changed），
// 并持有本实例模块的内置事件声明（由 EventRuntime::register_builtin_events 接进事件引擎）；
// 断开时 shutdown() 注销数值模块、清除内置声明并清掉引擎运行期状态。
// 爪印 D0 的 10Hz 全量物理数据默认不写入数值（避免 10Hz 触发规则求值），
// 只有配置 app.devices.paw.d0_values 为 true 时才落值；
// 灵猫气压每 100ms 上报一次，只在变化达到 app.devices.civet.pressure_epsilon（单位 0.01 kPa）
// 时才写 pressure / raw，其余上报丢弃并计数，避免 10Hz 触发规则求值。
class DeviceValueBridge : public QObject {
    Q_OBJECT

public:
    /// @brief 设备种类（决定注册哪一份数值表与内置事件声明）
    enum class DeviceKind {
        PawPrints, ///< 爪印无线按钮传感器
        Civet,     ///< 灵猫边缘控制传感器
    };

    /// @brief 构造单台设备的数值桥接（不访问 ModuleManager 与配置）
    /// @param kind 设备种类
    /// @param module_name 实例数值模块名（如 paw_430C，同时是内置事件声明的模块标识）
    /// @param display_name 实例显示名（如 爪印 430C，用于日志与文档）
    /// @param driver 驱动实例（爪印或灵猫，可为空：为空时只注册数值与事件声明）
    /// @param parent 父对象
    DeviceValueBridge(DeviceKind kind, std::string module_name, std::string display_name,
        QObject* driver = nullptr, QObject* parent = nullptr);

    ~DeviceValueBridge() override;

    /// @brief 注入事件运行时：内置声明在 initialize() 注册、shutdown() 注销（不接管生命周期）
    /// @param runtime 事件运行时，可为空
    void attach_event_runtime(EventRuntime* runtime) { event_runtime_ = runtime; }

    /// @brief 迁移旧配置映射、注册实例数值、写入初始基线、注册内置声明并连接驱动信号
    void initialize();

    /// @brief 注销实例数值模块与内置事件声明（引擎运行期状态一并清掉），重复调用无副作用
    void shutdown();

    /// @brief 实例数值模块名（同时是内置事件声明的模块标识）
    const std::string& module_name() const { return module_name_; }

    /// @brief 实例显示名（如 爪印 430C）
    const std::string& display_name() const { return display_name_; }

    /// @brief 是否已注销
    bool shut_down() const { return shut_down_; }

    /// @brief `paw` 模块的内置事件声明（按键按下 / 抬起）
    /// @return 事件声明列表；头文件内联，便于单元测试在不链接驱动的情况下核对声明内容
    static std::vector<EventDefinition> builtin_events() {
        EventDefinition pressed;
        pressed.id = "button_pressed";
        pressed.label = "按键按下";
        pressed.trigger.value_id = "button";
        pressed.trigger.compare = EventCompare::INCREASE;
        pressed.trigger.amount = 1;

        EventDefinition released;
        released.id = "button_released";
        released.label = "按键抬起";
        released.trigger.value_id = "button";
        released.trigger.compare = EventCompare::DECREASE;
        released.trigger.amount = 1;

        return {pressed, released};
    }

    /// @brief `civet` 模块的内置事件声明（气压上升 / 下降）
    /// @return 事件声明列表；阈值由规则配置，事件只区分上升与下降
    static std::vector<EventDefinition> civet_builtin_events() {
        EventDefinition rise;
        rise.id = "pressure_rise";
        rise.label = "气压上升";
        rise.trigger.value_id = "pressure";
        rise.trigger.compare = EventCompare::INCREASE;
        rise.trigger.amount = 1;

        EventDefinition drop;
        drop.id = "pressure_drop";
        drop.label = "气压下降";
        drop.trigger.value_id = "pressure";
        drop.trigger.compare = EventCompare::DECREASE;
        drop.trigger.amount = 1;

        return {rise, drop};
    }

    static constexpr const char* kModuleName = "paw";      ///< 爪印的旧模块标识（单实例时代的模块名，仅用于旧配置迁移与声明核对）
    static constexpr const char* kCivetModuleName = "civet"; ///< 灵猫的旧模块标识（同上）
    static constexpr int kCivetDefaultEpsilon = 10;        ///< 气压落值阈值缺省值（10 个 0.01 kPa，即 0.1 kPa）

    /// @brief D0 物理数据是否写入数值（配置 app.devices.paw.d0_values，缺省 false）
    bool d0_values_enabled() const { return d0_values_; }

    /// @brief 气压落值阈值（配置 app.devices.civet.pressure_epsilon，缺省 10，单位 0.01 kPa）
    int civet_epsilon() const { return civet_epsilon_; }

    /// @brief 因未达阈值被丢弃的气压上报数
    int civet_suppressed() const { return civet_suppressed_; }

private:
    void initialize_paw();
    void initialize_civet();
    void register_builtin_events();
    void migrate_legacy_mapping();
    PawPrintsBleController* paw_driver() const;
    CivetBleController* civet_driver() const;
    void on_device_info(int indicator_color, int device_type, int battery);
    void on_trigger_started(int event_id, int param);
    void on_trigger_cancelled(int event_id);
    void on_param_changed(int event_id, int param);
    void on_physical_data(int pressed, int accel, int angle_x, int angle_y, int angle_z, int voltage);
    void set_value(const char* value_id, int value);
    void on_civet_pressure(int pressure_raw);
    void on_civet_battery(int percent);
    void set_civet_value(const char* value_id, int value);

    DeviceKind kind_ = DeviceKind::PawPrints; ///< 设备种类
    std::string module_name_;                ///< 实例数值模块名（paw_430C 之类）
    std::string display_name_;               ///< 实例显示名（爪印 430C 之类）
    QObject* driver_ = nullptr;              ///< 驱动实例（可为空）
    EventRuntime* event_runtime_ = nullptr;  ///< 事件运行时（可为空：为空时不动内置声明）
    bool shut_down_ = false;                 ///< 是否已注销
    bool d0_values_ = false;                 ///< D0 物理数据是否落值
    bool d0_skipped_logged_ = false;         ///< D0 未落值是否已记录一次
    int civet_epsilon_ = kCivetDefaultEpsilon; ///< 气压落值阈值（0.01 kPa）
    int civet_last_pressure_ = 0;           ///< 上一次落值的气压原始值
    int civet_suppressed_ = 0;              ///< 未达阈值被丢弃的气压上报数
    bool civet_suppressed_logged_ = false;  ///< 阈值丢帧是否已记录一次
};
