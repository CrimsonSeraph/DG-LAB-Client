/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 爪印输入侧测试：50 指令构造、51/5A/5B/5C/D0/F1 解析与 F1 的 /2 展示、数值钳制，
// 以及内置事件声明进入 EventRuntime 的最小链路（注册 → 载入 → 数值变化触发）
// 配置与通道上限在临时目录内构造，不读写仓库里的 config/

#include "AppConfig.h"
#include "CoyoteBleController.h"
#include "DeviceValueBridge.h"
#include "EventMapping.h"
#include "EventRuntime.h"
#include "ModuleManager.h"
#include "PawFrames.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QTemporaryDir>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace {

    int failures = 0; ///< 失败断言数

    // 记录一次断言失败
    void fail(const char* case_name, const char* detail, int line) {
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", case_name, line, detail);
        ++failures;
    }

    // 整型断言
    void check_int(const char* case_name, int expected, int actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %d actual %d", expr, expected, actual);
        fail(case_name, detail, line);
    }

    // 布尔断言
    void check_true(const char* case_name, bool actual, const char* expr, int line) {
        if (actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected true actual false", expr);
        fail(case_name, detail, line);
    }

    // 字节串断言（按 HEX 文本比较）
    void check_text(const char* case_name, const QString& expected, const QString& actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[512] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %s actual %s", expr, expected.toUtf8().constData(),
            actual.toUtf8().constData());
        fail(case_name, detail, line);
    }

    // 50 指令布局：头 + 指示灯色 + 15 字节触发设置（缺省为按下/抬起触发）
    void test_config_layout() {
        const QByteArray settings = paw_frames::trigger_settings_button(paw_frames::ButtonTriggerSettings());
        check_int("settings-length", 15, settings.size(), "trigger_settings_button().size()", __LINE__);
        const QByteArray frame = paw_frames::build_config(paw_frames::kDefaultIndicatorColor, settings);
        check_int("config-length", 17, frame.size(), "build_config().size()", __LINE__);
        check_text("config-full", QStringLiteral("500105010000ff00ff0000000000000000"),
            QString::fromLatin1(frame.toHex()), "build_config().toHex()", __LINE__);
        check_int("config-head", 0x50, static_cast<quint8>(frame.at(0)), "frame.at(0)", __LINE__);
        check_int("config-color", 0x01, static_cast<quint8>(frame.at(1)), "frame.at(1)", __LINE__);
        check_int("config-mode", paw_frames::kModeButton, static_cast<quint8>(frame.at(2)), "frame.at(2)", __LINE__);
        check_int("config-event-id", 0x01, static_cast<quint8>(frame.at(3)), "frame.at(3)", __LINE__);
        check_int("config-settings-byte", 0x00, static_cast<quint8>(frame.at(4)), "frame.at(4)", __LINE__);
        check_int("config-param-up", 0xFF, static_cast<quint8>(frame.at(8)), "frame.at(8)", __LINE__);
        check_int("config-motion-event", 0x00, static_cast<quint8>(frame.at(9)), "frame.at(9)", __LINE__);
        check_int("config-map-range", 0x00, static_cast<quint8>(frame.at(16)), "frame.at(16)", __LINE__);
    }

    // 触发设置长度异常：15 字节以外一律拒绝，不猜布局
    void test_config_rejects_bad_settings() {
        check_true("config-rejects-short", paw_frames::build_config(0x01, QByteArray(14, '\0')).isEmpty(),
            "build_config(14 bytes).isEmpty()", __LINE__);
        check_true("config-rejects-long", paw_frames::build_config(0x01, QByteArray(16, '\0')).isEmpty(),
            "build_config(16 bytes).isEmpty()", __LINE__);
        check_true("config-rejects-empty", paw_frames::build_config(0x01, QByteArray()).isEmpty(),
            "build_config(empty).isEmpty()", __LINE__);
        check_int("mode-none-length", 15, paw_frames::trigger_settings_none().size(),
            "trigger_settings_none().size()", __LINE__);
        check_text("mode-none-bytes", QStringLiteral("000000000000000000000000000000"),
            QString::fromLatin1(paw_frames::trigger_settings_none().toHex()), "trigger_settings_none()", __LINE__);
    }

    // 设置集合字节：按文档三个示例的位布局核对（bit7 抬起 / bit6 角度子模式 / bit5~3 / bit2~0）
    void test_settings_byte() {
        check_int("byte-press", 0x00, paw_frames::button_settings_byte(false, false, 0, 0),
            "button_settings_byte(false,false,0,0)", __LINE__);
        check_int("byte-doc-example-accel", 0x1E, paw_frames::button_settings_byte(false, false, 3, 6),
            "button_settings_byte(false,false,3,6)", __LINE__);
        check_int("byte-doc-example-angle", 0x5E, paw_frames::button_settings_byte(false, true, 3, 6),
            "button_settings_byte(false,true,3,6)", __LINE__);
        check_int("byte-doc-example-release", 0x80, paw_frames::button_settings_byte(true, false, 0, 0),
            "button_settings_byte(true,false,0,0)", __LINE__);
        check_int("byte-doc-example-combined", 0x9E, paw_frames::button_settings_byte(true, false, 3, 6),
            "button_settings_byte(true,false,3,6)", __LINE__);
        check_int("byte-debounce-clamp", 0x3F, paw_frames::button_settings_byte(false, false, 9, 9),
            "button_settings_byte(false,false,9,9)", __LINE__);
    }

    // 按钮触发设置：自定义参数落在文档规定的位置上，阈值长度异常整条拒绝
    void test_button_settings() {
        paw_frames::ButtonTriggerSettings settings;
        settings.event_id = 0x14;
        settings.settings = paw_frames::button_settings_byte(true, false, 3, 6);
        settings.param_up_speed = 0x14;
        settings.param_cancel_down = 0x32;
        settings.param_down_speed = 0x14;
        settings.param_up_value = 0x14;
        const QByteArray bytes = paw_frames::trigger_settings_button(settings);
        check_int("button-settings-length", 15, bytes.size(), "trigger_settings_button().size()", __LINE__);
        check_text("button-settings-head", QStringLiteral("05149e14321414"),
            QString::fromLatin1(bytes.left(7).toHex()), "trigger_settings_button().left(7)", __LINE__);

        settings.motion_thresholds = QByteArray(5, '\0');
        check_true("button-settings-rejects-thresholds", paw_frames::trigger_settings_button(settings).isEmpty(),
            "trigger_settings_button(5 bytes).isEmpty()", __LINE__);
    }

    // 51 消息：指示灯颜色 / 设备类型 / 电量，长度不符按未识别丢弃
    void test_parse_device_info() {
        const paw_frames::Message message = paw_frames::parse_message(QByteArray::fromHex("51030355"));
        check_int("51-kind", static_cast<int>(paw_frames::MessageKind::DeviceInfo),
            static_cast<int>(message.kind), "parse 51 kind", __LINE__);
        check_int("51-color", 0x03, message.indicator_color, "51 indicator_color", __LINE__);
        check_int("51-type", paw_frames::kDeviceTypePawV11, message.device_type, "51 device_type", __LINE__);
        check_int("51-battery", 0x55, message.battery, "51 battery", __LINE__);
        check_int("51-short", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("5103")).kind), "51 short kind", __LINE__);
        check_int("51-long", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("5103035500")).kind), "51 long kind",
            __LINE__);
    }

    // 5A / 5B / 5C：触发、取消与参数变化，长度不符按未识别丢弃
    void test_parse_trigger_messages() {
        const paw_frames::Message start = paw_frames::parse_message(QByteArray::fromHex("5a020764"));
        check_int("5a-kind", static_cast<int>(paw_frames::MessageKind::TriggerStart), static_cast<int>(start.kind),
            "5a kind", __LINE__);
        check_int("5a-event", 7, start.event_id, "5a event_id", __LINE__);
        check_int("5a-param", 100, start.param, "5a param", __LINE__);

        const paw_frames::Message cancel = paw_frames::parse_message(QByteArray::fromHex("5b0207"));
        check_int("5b-kind", static_cast<int>(paw_frames::MessageKind::TriggerCancel), static_cast<int>(cancel.kind),
            "5b kind", __LINE__);
        check_int("5b-event", 7, cancel.event_id, "5b event_id", __LINE__);
        check_int("5b-param-absent", -1, cancel.param, "5b param", __LINE__);

        const paw_frames::Message change = paw_frames::parse_message(QByteArray::fromHex("5c020750"));
        check_int("5c-kind", static_cast<int>(paw_frames::MessageKind::ParamChange), static_cast<int>(change.kind),
            "5c kind", __LINE__);
        check_int("5c-event", 7, change.event_id, "5c event_id", __LINE__);
        check_int("5c-param", 80, change.param, "5c param", __LINE__);

        check_int("5a-short", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("5a0207")).kind), "5a short kind",
            __LINE__);
        check_int("5b-long", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("5b020700")).kind), "5b long kind",
            __LINE__);
    }

    // D0 消息：9 字节全量物理数据，越界长度按未识别丢弃
    void test_parse_physical() {
        const paw_frames::Message message = paw_frames::parse_message(QByteArray::fromHex("d0030a011405fb0ac8"));
        check_int("d0-kind", static_cast<int>(paw_frames::MessageKind::Physical), static_cast<int>(message.kind),
            "d0 kind", __LINE__);
        check_int("d0-color", 0x03, message.indicator_color, "d0 indicator_color", __LINE__);
        check_int("d0-sequence", 10, message.sequence, "d0 sequence", __LINE__);
        check_int("d0-pressed", 1, message.pressed, "d0 pressed", __LINE__);
        check_int("d0-accel", 20, message.accel, "d0 accel", __LINE__);
        check_int("d0-angle-x", 5, message.angle_x, "d0 angle_x", __LINE__);
        check_int("d0-angle-y", 251, message.angle_y, "d0 angle_y", __LINE__);
        check_int("d0-angle-z", 10, message.angle_z, "d0 angle_z", __LINE__);
        check_int("d0-voltage", 200, message.voltage, "d0 voltage", __LINE__);
        check_int("d0-short", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("d0030a011405fb0a")).kind),
            "d0 short kind", __LINE__);
    }

    // F1 消息：有符号 2 字节阈值与 /2 展示值（取整规则未定，按有符号整数截断）
    void test_parse_angle_self_test() {
        const paw_frames::Message message =
            paw_frames::parse_message(QByteArray::fromHex("f161ff9c0032ff80007f0000fffe"));
        check_int("f1-kind", static_cast<int>(paw_frames::MessageKind::AngleSelfTest), static_cast<int>(message.kind),
            "f1 kind", __LINE__);
        check_int("f1-x-low", -100, message.threshold_low[0], "f1 x low", __LINE__);
        check_int("f1-x-high", 50, message.threshold_high[0], "f1 x high", __LINE__);
        check_int("f1-y-low", -128, message.threshold_low[1], "f1 y low", __LINE__);
        check_int("f1-y-high", 127, message.threshold_high[1], "f1 y high", __LINE__);
        check_int("f1-z-low", 0, message.threshold_low[2], "f1 z low", __LINE__);
        check_int("f1-z-high", -2, message.threshold_high[2], "f1 z high", __LINE__);
        check_int("f1-x-low-half", -50, paw_frames::halve_threshold(message.threshold_low[0]), "halve x low",
            __LINE__);
        check_int("f1-x-high-half", 25, paw_frames::halve_threshold(message.threshold_high[0]), "halve x high",
            __LINE__);
        check_int("f1-y-low-half", -64, paw_frames::halve_threshold(message.threshold_low[1]), "halve y low",
            __LINE__);
        check_int("f1-y-high-half", 63, paw_frames::halve_threshold(message.threshold_high[1]), "halve y high",
            __LINE__);
        check_int("f1-z-high-half", -1, paw_frames::halve_threshold(message.threshold_high[2]), "halve z high",
            __LINE__);

        check_int("f1-fixed-byte", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("f162ff9c0032ff80007f0000fffe")).kind),
            "f1 fixed byte kind", __LINE__);
        check_int("f1-short", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("f161ff9c0032ff80007f0000ff")).kind),
            "f1 short kind", __LINE__);
    }

    // 未识别消息：空、未知头、只有头
    void test_parse_unknown() {
        check_int("unknown-empty", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray()).kind), "parse empty kind", __LINE__);
        check_int("unknown-head", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("7f0000")).kind), "parse 7f kind",
            __LINE__);
        check_int("unknown-only-head", static_cast<int>(paw_frames::MessageKind::Unknown),
            static_cast<int>(paw_frames::parse_message(QByteArray::fromHex("51")).kind), "parse 51 only kind",
            __LINE__);
    }

    // 数值桥钳制：按登记范围钳制，未登记 id 原样返回，注册列表范围与定义表一致
    void test_value_clamp() {
        check_int("clamp-count", 9, paw_values::kValueCount, "kValueCount", __LINE__);
        check_int("clamp-button-high", 1, paw_values::clamp("button", 5), "clamp button 5", __LINE__);
        check_int("clamp-button-low", 0, paw_values::clamp("button", -1), "clamp button -1", __LINE__);
        check_int("clamp-param-high", 255, paw_values::clamp("param", 300), "clamp param 300", __LINE__);
        check_int("clamp-angle-low", -128, paw_values::clamp("angle_x", -200), "clamp angle_x -200", __LINE__);
        check_int("clamp-angle-high", 127, paw_values::clamp("angle_z", 200), "clamp angle_z 200", __LINE__);
        check_int("clamp-battery", 100, paw_values::clamp("battery", 150), "clamp battery 150", __LINE__);
        check_int("clamp-event-id", 24, paw_values::clamp("event_id", 99), "clamp event_id 99", __LINE__);
        check_int("clamp-unknown", 999, paw_values::clamp("ghost", 999), "clamp ghost 999", __LINE__);
        check_true("clamp-unknown-absent", paw_values::find("ghost") == nullptr, "find ghost == nullptr", __LINE__);

        const std::vector<ModuleValue> values = paw_values::build();
        check_int("build-count", paw_values::kValueCount, static_cast<int>(values.size()), "build().size()", __LINE__);
        for (const ModuleValue& value : values) {
            const paw_values::ValueSpec* spec = paw_values::find(value.get_id());
            if (spec == nullptr) {
                fail("build-spec-missing", value.get_id().c_str(), __LINE__);
                continue;
            }
            check_int("build-min", spec->min, value.clamp_value(spec->min - 10), "clamp_value(min-10)", __LINE__);
            check_int("build-max", spec->max, value.clamp_value(spec->max + 10), "clamp_value(max+10)", __LINE__);
        }
    }

    // 内置事件声明：注册 → 载入 → 数值变化触发，重复注册不累积，空声明可清除
    void test_builtin_events_into_runtime() {
        QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_paw_device_XXXXXX"));
        if (!temp.isValid()) {
            fail("temp-dir", "临时目录创建失败", __LINE__);
            return;
        }
        const std::string config_dir = temp.path().toStdString() + "/config";
        std::filesystem::create_directories(config_dir);
        if (!AppConfig::instance().initialize(config_dir)) {
            fail("config-init", "配置初始化失败", __LINE__);
            return;
        }
        EventChannelConfig channel_config;
        channel_config.limit = 200;
        channel_config.safety = 150;
        write_channel_config("A", channel_config);
        write_channel_config("B", channel_config);

        check_true("paw-values-register",
            ModuleManager::instance().register_module_values(DeviceValueBridge::kModuleName, paw_values::build(), {}),
            "register_module_values(paw)", __LINE__);

        EventRuntime runtime;
        runtime.register_builtin_events(DeviceValueBridge::kModuleName, DeviceValueBridge::builtin_events());
        check_int("builtin-source-count", 1, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size()", __LINE__);
        // 重复注册同一模块标识整体替换，不累积出第二份声明
        runtime.register_builtin_events(DeviceValueBridge::kModuleName, DeviceValueBridge::builtin_events());
        check_int("builtin-source-no-dup", 1, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size() after re-register", __LINE__);

        // initialize() 建立 ModuleManager::value_changed → 引擎的连接，并载入插件声明与内置声明
        runtime.initialize();
        check_true("builtin-module-loaded", runtime.engine().has_module(DeviceValueBridge::kModuleName),
            "engine().has_module(paw)", __LINE__);

        // 运行期启用集合覆盖映射的 enabled：只为 button_pressed 启用
        std::set<std::string> cascade;
        cascade.insert("paw/button_pressed");
        runtime.set_cascade_events(cascade);

        ModuleManager::instance().set_value("paw", "button", 0);
        check_int("builtin-first-value-no-trigger", 0,
            static_cast<int>(runtime.engine().recent_triggers(200).size()), "triggers after first write", __LINE__);
        ModuleManager::instance().set_value("paw", "button", 1);
        check_int("builtin-pressed-trigger", 1, static_cast<int>(runtime.engine().recent_triggers(200).size()),
            "triggers after press", __LINE__);
        ModuleManager::instance().set_value("paw", "button", 0);
        check_int("builtin-released-disabled", 1, static_cast<int>(runtime.engine().recent_triggers(200).size()),
            "triggers after release (not enabled)", __LINE__);

        // 空声明清除该模块的内置来源
        runtime.register_builtin_events(DeviceValueBridge::kModuleName, {});
        check_true("builtin-cleared", !runtime.engine().has_module(DeviceValueBridge::kModuleName),
            "engine().has_module(paw) after clear", __LINE__);
    }

    // 型号识别三态：47L120 实验性且可连接、47L120100 需先升级固件且不可连接、47L121 已支持
    void test_identify_models() {
        const CoyoteBleController::DeviceProfile v11 =
            CoyoteBleController::identify(QStringLiteral("47L120300"));
        check_text("identify-paw-model", QStringLiteral("47L120"), v11.model, "identify 47L120300 model",
            __LINE__);
        check_int("identify-paw-state", static_cast<int>(IOutputDevice::SupportState::Experimental),
            static_cast<int>(v11.state), "identify 47L120300 state", __LINE__);
        check_true("identify-paw-connectable", v11.supported, "identify 47L120300 supported", __LINE__);
        check_true("identify-paw-input-only", !v11.output, "identify 47L120300 !output", __LINE__);
        check_true("identify-paw-type", v11.type == QStringLiteral("pawprint"), "identify pawprint type",
            __LINE__);
        check_true("identify-paw-hint-empty", v11.hint.isEmpty(), "identify 47L120300 hint empty", __LINE__);

        const CoyoteBleController::DeviceProfile v10 =
            CoyoteBleController::identify(QStringLiteral("47L120100"));
        check_text("identify-paw-v10-model", QStringLiteral("47L120100"), v10.model, "identify V1.0 model",
            __LINE__);
        check_int("identify-paw-v10-state", static_cast<int>(IOutputDevice::SupportState::Unsupported),
            static_cast<int>(v10.state), "identify V1.0 state", __LINE__);
        check_true("identify-paw-v10-blocked", !v10.supported, "identify V1.0 !supported", __LINE__);
        check_true("identify-paw-v10-hint", v10.hint.contains(QStringLiteral("需先升级固件")),
            "identify V1.0 hint", __LINE__);

        const CoyoteBleController::DeviceProfile coyote =
            CoyoteBleController::identify(QStringLiteral("47L121000"));
        check_int("identify-coyote-state", static_cast<int>(IOutputDevice::SupportState::Supported),
            static_cast<int>(coyote.state), "identify 47L121000 state", __LINE__);
        check_true("identify-coyote-output", coyote.output && coyote.supported,
            "identify 47L121000 output && supported", __LINE__);
    }

    // 全部用例入口：返回失败断言数
    int run_all() {
        test_config_layout();
        test_config_rejects_bad_settings();
        test_settings_byte();
        test_button_settings();
        test_parse_device_info();
        test_parse_trigger_messages();
        test_parse_physical();
        test_parse_angle_self_test();
        test_parse_unknown();
        test_value_clamp();
        test_identify_models();
        test_builtin_events_into_runtime();
        return failures;
    }

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const int failed = run_all();
    if (failed == 0) {
        std::printf("paw_device_test: all cases passed\n");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "paw_device_test: %d assertion(s) failed\n", failed);
    return EXIT_FAILURE;
}
