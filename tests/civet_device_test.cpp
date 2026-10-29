/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 灵猫输入侧测试：50 指令构造、D0 气压的宽容解析与换算、66 指令的 13 / 14 两形态、
// 气压落值阈值抑制、数值钳制，以及内置事件声明进入 EventRuntime 的最小链路（注册 → 载入 → 数值变化触发）
// 配置与通道上限在临时目录内构造，不读写仓库里的 config/

#include "AppConfig.h"
#include "CivetFrames.h"
#include "CoyoteBleController.h"
#include "DeviceValueBridge.h"
#include "EventMapping.h"
#include "EventRuntime.h"
#include "ModuleManager.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QTemporaryDir>

#include <cmath>
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
    void check_text(const char* case_name, const QString& expected, const QString& actual, const char* expr,
        int line) {
        if (expected == actual) {
            return;
        }
        char detail[512] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %s actual %s", expr, expected.toUtf8().constData(),
            actual.toUtf8().constData());
        fail(case_name, detail, line);
    }

    // 浮点断言（气压换算按 0.01 kPa 精度比较）
    void check_double(const char* case_name, double expected, double actual, const char* expr, int line) {
        if (std::fabs(expected - actual) < 1e-6) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %.4f actual %.4f", expr, expected, actual);
        fail(case_name, detail, line);
    }

    // 50 指令：17 字节，头 + 指示灯色 + 开启/停止 + 14 字节 0x00
    void test_report_command() {
        const QByteArray start = civet_frames::build_report_start();
        check_int("50-start-length", 17, start.size(), "build_report_start().size()", __LINE__);
        check_text("50-start-bytes", QStringLiteral("5001d0") + QStringLiteral("00").repeated(14),
            QString::fromLatin1(start.toHex()), "build_report_start().toHex()", __LINE__);
        check_int("50-start-head", 0x50, static_cast<quint8>(start.at(0)), "frame.at(0)", __LINE__);
        check_int("50-start-color", 0x01, static_cast<quint8>(start.at(1)), "frame.at(1)", __LINE__);
        check_int("50-start-flag", civet_frames::kReportStart, static_cast<quint8>(start.at(2)), "frame.at(2)",
            __LINE__);

        const QByteArray stop = civet_frames::build_report_stop();
        check_int("50-stop-length", 17, stop.size(), "build_report_stop().size()", __LINE__);
        check_text("50-stop-bytes", QStringLiteral("500100") + QStringLiteral("00").repeated(14),
            QString::fromLatin1(stop.toHex()), "build_report_stop().toHex()", __LINE__);
        check_int("50-stop-flag", civet_frames::kReportStop, static_cast<quint8>(stop.at(2)), "stop.at(2)",
            __LINE__);

        // 自定义指示灯颜色只改第 2 字节，其余布局不变
        const QByteArray custom = civet_frames::build_report_command(0x03, true);
        check_int("50-custom-color", 0x03, static_cast<quint8>(custom.at(1)), "custom.at(1)", __LINE__);
        check_int("50-custom-length", 17, custom.size(), "custom.size()", __LINE__);
    }

    // 66 指令：13 字节形态与文档示例字面值一致，14 字节为回退形态
    void test_command66() {
        const QByteArray reset = civet_frames::build_command66(civet_frames::kScreenRotateKeep,
            civet_frames::kPressureResetWord);
        check_int("66-length", 13, reset.size(), "build_command66().size()", __LINE__);
        check_text("66-reset-bytes", QStringLiteral("0x66000000000000000000000002").mid(2),
            QString::fromLatin1(reset.toHex()), "build_command66().toHex()", __LINE__);
        check_int("66-head", 0x66, static_cast<quint8>(reset.at(0)), "reset.at(0)", __LINE__);
        check_int("66-rotate-keep", civet_frames::kScreenRotateKeep, static_cast<quint8>(reset.at(10)),
            "reset.at(10)", __LINE__);
        check_int("66-reset-word-high", 0x00, static_cast<quint8>(reset.at(11)), "reset.at(11)", __LINE__);
        check_int("66-reset-word-low", 0x02, static_cast<quint8>(reset.at(12)), "reset.at(12)", __LINE__);

        // 只翻转方向：重置值为 0，与文档示例模板（screenRotate 换成 0x03）逐字节一致
        const QByteArray rotate = civet_frames::build_command66(civet_frames::kScreenRotateFlipped,
            civet_frames::kPressureKeepWord);
        check_int("66-rotate-length", 13, rotate.size(), "build_command66(rotate).size()", __LINE__);
        check_text("66-rotate-bytes", QStringLiteral("66") + QStringLiteral("00").repeated(9)
                + QStringLiteral("030000"),
            QString::fromLatin1(rotate.toHex()), "build_command66(rotate).toHex()", __LINE__);
        check_int("66-rotate-value", civet_frames::kScreenRotateFlipped, static_cast<quint8>(rotate.at(10)),
            "rotate.at(10)", __LINE__);

        const QByteArray wide = civet_frames::build_command66_wide(civet_frames::kScreenRotateKeep,
            civet_frames::kPressureResetWord);
        check_int("66-wide-length", 14, wide.size(), "build_command66_wide().size()", __LINE__);
        check_text("66-wide-bytes", QStringLiteral("66") + QStringLiteral("00").repeated(12)
                + QStringLiteral("02"),
            QString::fromLatin1(wide.toHex()), "build_command66_wide().toHex()", __LINE__);
    }

    // D0 气压消息：文档标称 17 字节、示例串实际 15 字节，解析按实际长度宽容
    void test_parse_report() {
        // 文档示例一：0xD001xxxxxxxxxxxx1603xxxxxxxxxx（15 字节），气压 0x0316 = 790 → 7.9 kPa
        const QByteArray doc15 = QByteArray::fromHex("d0010000000000001603" "0000000000");
        const civet_frames::PressureReport report = civet_frames::parse_report(doc15);
        check_true("d0-15-valid", report.valid, "parse 15 bytes valid", __LINE__);
        check_int("d0-15-size", 15, report.size, "report.size", __LINE__);
        check_int("d0-15-color", 0x01, report.indicator_color, "report.indicator_color", __LINE__);
        check_int("d0-15-raw", 790, report.pressure_raw, "report.pressure_raw", __LINE__);
        check_double("d0-15-kpa", 7.9, civet_frames::pressure_kpa(report.pressure_raw), "pressure_kpa(790)",
            __LINE__);

        // 文档示例二：0xA806 = 1704 → 17.04 kPa
        const QByteArray doc_a806 = QByteArray::fromHex("d001000000000000a806" "0000000000");
        check_int("d0-a806-raw", 1704, civet_frames::parse_report(doc_a806).pressure_raw, "a806 raw", __LINE__);
        check_double("d0-a806-kpa", 17.04, civet_frames::pressure_kpa(1704), "pressure_kpa(1704)", __LINE__);

        // 标称 17 字节：多出的填充字节不影响气压位置
        const QByteArray full17 = QByteArray::fromHex("d0010000000000001603" "00000000000000");
        const civet_frames::PressureReport full = civet_frames::parse_report(full17);
        check_true("d0-17-valid", full.valid, "parse 17 bytes valid", __LINE__);
        check_int("d0-17-size", 17, full.size, "full.size", __LINE__);
        check_int("d0-17-raw", 790, full.pressure_raw, "full.pressure_raw", __LINE__);

        // 长度不足 10 字节：整帧丢弃（valid=false），驱动的 dropped_reports 由此计数
        check_true("d0-9-invalid", !civet_frames::parse_report(QByteArray::fromHex("d00100000000000016")).valid,
            "parse 9 bytes invalid", __LINE__);
        check_true("d0-10-valid", civet_frames::parse_report(QByteArray::fromHex("d0010000000000001603")).valid,
            "parse 10 bytes valid", __LINE__);
        check_true("d0-empty-invalid", !civet_frames::parse_report(QByteArray()).valid, "parse empty invalid",
            __LINE__);

        // 非法/边界 int16：按有符号小端原样上报，不做量程假设
        check_int("d0-negative-one",
            -1, civet_frames::parse_report(QByteArray::fromHex("d001000000000000ffff0000")).pressure_raw,
            "0xffff raw", __LINE__);
        check_int("d0-negative-min",
            -32768, civet_frames::parse_report(QByteArray::fromHex("d00100000000000000800000")).pressure_raw,
            "0x8000 raw", __LINE__);
        check_int("d0-positive-max",
            32767, civet_frames::parse_report(QByteArray::fromHex("d001000000000000ff7f0000")).pressure_raw,
            "0x7fff raw", __LINE__);
        check_int("d0-little-endian", 0x0316,
            civet_frames::parse_report(QByteArray::fromHex("d0010000000000001603")).pressure_raw,
            "1603 little endian", __LINE__);

        // 头不符的消息不当成气压帧
        check_true("d0-head-mismatch", !civet_frames::parse_report(QByteArray::fromHex("5001d000000000001603000000")).valid,
            "parse 0x50 head invalid", __LINE__);
    }

    // 电量特征：1 字节百分比，空值与越界值有确定行为
    void test_battery() {
        check_int("battery-55", 85, civet_frames::battery_percent(QByteArray::fromHex("55")), "battery 0x55",
            __LINE__);
        check_int("battery-clamp", 100, civet_frames::battery_percent(QByteArray::fromHex("ff")), "battery 0xff",
            __LINE__);
        check_int("battery-zero", 0, civet_frames::battery_percent(QByteArray::fromHex("00")), "battery 0x00",
            __LINE__);
        check_int("battery-empty", civet_frames::kBatteryUnknown, civet_frames::battery_percent(QByteArray()),
            "battery empty", __LINE__);
    }

    // 气压落值阈值：达到阈值才写值，阈值 0 退化为「与原值不同即落值」
    void test_epsilon() {
        check_true("epsilon-below", !civet_values::pressure_changed(0, 9, 10), "pressure_changed(0,9,10)",
            __LINE__);
        check_true("epsilon-equal", civet_values::pressure_changed(0, 10, 10), "pressure_changed(0,10,10)",
            __LINE__);
        check_true("epsilon-above", civet_values::pressure_changed(0, 11, 10), "pressure_changed(0,11,10)",
            __LINE__);
        check_true("epsilon-negative-delta", civet_values::pressure_changed(0, -10, 10),
            "pressure_changed(0,-10,10)", __LINE__);
        check_true("epsilon-drop", civet_values::pressure_changed(790, 779, 10), "pressure_changed(790,779,10)",
            __LINE__);
        check_true("epsilon-drop-below", !civet_values::pressure_changed(790, 781, 10),
            "pressure_changed(790,781,10)", __LINE__);
        check_true("epsilon-same", !civet_values::pressure_changed(790, 790, 10), "pressure_changed(790,790,10)",
            __LINE__);
        check_true("epsilon-zero-differs", civet_values::pressure_changed(5, 6, 0), "pressure_changed(5,6,0)",
            __LINE__);
        check_true("epsilon-zero-same", !civet_values::pressure_changed(5, 5, 0), "pressure_changed(5,5,0)",
            __LINE__);
        check_int("epsilon-default", 10, DeviceValueBridge::kCivetDefaultEpsilon, "kCivetDefaultEpsilon",
            __LINE__);
    }

    // 数值桥钳制：按登记范围钳制，未登记 id 原样返回，注册列表范围与定义表一致
    void test_value_clamp() {
        check_int("clamp-count", 3, civet_values::kValueCount, "kValueCount", __LINE__);
        check_int("clamp-pressure-high", 32767, civet_values::clamp("pressure", 40000), "clamp pressure 40000",
            __LINE__);
        check_int("clamp-pressure-low", -32768, civet_values::clamp("pressure", -40000), "clamp pressure -40000",
            __LINE__);
        check_int("clamp-raw-passthrough", 790, civet_values::clamp("raw", 790), "clamp raw 790", __LINE__);
        check_int("clamp-battery-high", 100, civet_values::clamp("battery", 150), "clamp battery 150", __LINE__);
        check_int("clamp-battery-low", 0, civet_values::clamp("battery", -1), "clamp battery -1", __LINE__);
        check_int("clamp-unknown", 999, civet_values::clamp("ghost", 999), "clamp ghost 999", __LINE__);
        check_true("clamp-unknown-absent", civet_values::find("ghost") == nullptr, "find ghost == nullptr",
            __LINE__);

        const std::vector<ModuleValue> values = civet_values::build();
        check_int("build-count", civet_values::kValueCount, static_cast<int>(values.size()), "build().size()",
            __LINE__);
        for (const ModuleValue& value : values) {
            const civet_values::ValueSpec* spec = civet_values::find(value.get_id());
            if (spec == nullptr) {
                fail("build-spec-missing", value.get_id().c_str(), __LINE__);
                continue;
            }
            check_int("build-min", spec->min, value.clamp_value(spec->min - 10), "clamp_value(min-10)", __LINE__);
            check_int("build-max", spec->max, value.clamp_value(spec->max + 10), "clamp_value(max+10)", __LINE__);
        }
    }

    // 型号识别三态：47L124 实验性且可连接、纯输入设备；郊狼与爪印的三态不受本批影响
    void test_identify_models() {
        const CoyoteBleController::DeviceProfile civet =
            CoyoteBleController::identify(QStringLiteral("47L124000"));
        check_text("identify-civet-model", QStringLiteral("47L124"), civet.model, "identify 47L124000 model",
            __LINE__);
        check_int("identify-civet-state", static_cast<int>(IOutputDevice::SupportState::Experimental),
            static_cast<int>(civet.state), "identify 47L124000 state", __LINE__);
        check_true("identify-civet-connectable", civet.supported, "identify 47L124000 supported", __LINE__);
        check_true("identify-civet-input-only", !civet.output, "identify 47L124000 !output", __LINE__);
        check_true("identify-civet-type", civet.type == QStringLiteral("civet"), "identify civet type", __LINE__);
        check_true("identify-civet-hint", civet.hint.contains(QStringLiteral("未经实机验证")),
            "identify 47L124000 hint", __LINE__);

        const CoyoteBleController::DeviceProfile coyote =
            CoyoteBleController::identify(QStringLiteral("47L121000"));
        check_int("identify-coyote-state", static_cast<int>(IOutputDevice::SupportState::Supported),
            static_cast<int>(coyote.state), "identify 47L121000 state", __LINE__);
        check_true("identify-coyote-output", coyote.output && coyote.supported,
            "identify 47L121000 output && supported", __LINE__);

        const CoyoteBleController::DeviceProfile paw =
            CoyoteBleController::identify(QStringLiteral("47L120300"));
        check_int("identify-paw-state", static_cast<int>(IOutputDevice::SupportState::Experimental),
            static_cast<int>(paw.state), "identify 47L120300 state", __LINE__);
        check_true("identify-paw-input-only", !paw.output, "identify 47L120300 !output", __LINE__);

        const CoyoteBleController::DeviceProfile unknown =
            CoyoteBleController::identify(QStringLiteral("47L199000"));
        check_int("identify-unknown-state", static_cast<int>(IOutputDevice::SupportState::Unsupported),
            static_cast<int>(unknown.state), "identify 47L199000 state", __LINE__);
    }

    // 内置事件声明：注册 → 载入 → 数值变化触发（上升与下降各一条），重复注册不累积
    void test_builtin_events_into_runtime() {
        QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_civet_device_XXXXXX"));
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

        check_true("civet-values-register",
            ModuleManager::instance().register_module_values(DeviceValueBridge::kCivetModuleName,
                civet_values::build(), {}),
            "register_module_values(civet)", __LINE__);

        const std::vector<EventDefinition> declarations = DeviceValueBridge::civet_builtin_events();
        check_int("civet-events-count", 2, static_cast<int>(declarations.size()), "civet_builtin_events().size()",
            __LINE__);
        check_text("civet-events-first", QStringLiteral("pressure_rise"), QString::fromStdString(declarations[0].id),
            "civet_builtin_events()[0].id", __LINE__);
        check_text("civet-events-second", QStringLiteral("pressure_drop"), QString::fromStdString(declarations[1].id),
            "civet_builtin_events()[1].id", __LINE__);

        EventRuntime runtime;
        runtime.register_builtin_events(DeviceValueBridge::kCivetModuleName,
            DeviceValueBridge::civet_builtin_events());
        check_int("builtin-source-count", 1, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size()", __LINE__);
        // 重复注册同一模块标识整体替换，不累积出第二份声明
        runtime.register_builtin_events(DeviceValueBridge::kCivetModuleName,
            DeviceValueBridge::civet_builtin_events());
        check_int("builtin-source-no-dup", 1, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size() after re-register", __LINE__);

        runtime.initialize();
        check_true("builtin-module-loaded", runtime.engine().has_module(DeviceValueBridge::kCivetModuleName),
            "engine().has_module(civet)", __LINE__);

        // 运行期启用集合覆盖映射的 enabled：只为 pressure_rise 启用
        std::set<std::string> cascade;
        cascade.insert("civet/pressure_rise");
        runtime.set_cascade_events(cascade);

        ModuleManager::instance().set_value(DeviceValueBridge::kCivetModuleName, "pressure", 0);
        check_int("builtin-baseline-no-trigger", 0,
            static_cast<int>(runtime.engine().recent_triggers(200).size()), "triggers after baseline",
            __LINE__);
        ModuleManager::instance().set_value(DeviceValueBridge::kCivetModuleName, "pressure", 790);
        check_int("builtin-rise-trigger", 1, static_cast<int>(runtime.engine().recent_triggers(200).size()),
            "triggers after rise", __LINE__);
        ModuleManager::instance().set_value(DeviceValueBridge::kCivetModuleName, "pressure", 400);
        check_int("builtin-drop-disabled", 1, static_cast<int>(runtime.engine().recent_triggers(200).size()),
            "triggers after drop (not enabled)", __LINE__);

        // 启用 pressure_drop：重建映射会丢弃引擎的前值记录，先重新登记一次基线再下降
        cascade.insert("civet/pressure_drop");
        runtime.set_cascade_events(cascade);
        ModuleManager::instance().set_value(DeviceValueBridge::kCivetModuleName, "pressure", 401);
        check_int("builtin-rebaseline-no-trigger", 1, static_cast<int>(runtime.engine().recent_triggers(200).size()),
            "triggers after rebaseline", __LINE__);
        ModuleManager::instance().set_value(DeviceValueBridge::kCivetModuleName, "pressure", 200);
        check_int("builtin-drop-trigger", 2, static_cast<int>(runtime.engine().recent_triggers(200).size()),
            "triggers after enabled drop", __LINE__);

        // 电量不是事件来源：写值不产生流水
        ModuleManager::instance().set_value(DeviceValueBridge::kCivetModuleName, "battery", 85);
        check_int("builtin-battery-no-trigger", 2, static_cast<int>(runtime.engine().recent_triggers(200).size()),
            "triggers after battery", __LINE__);

        // 空声明清除该模块的内置来源
        runtime.register_builtin_events(DeviceValueBridge::kCivetModuleName, {});
        check_true("builtin-cleared", !runtime.engine().has_module(DeviceValueBridge::kCivetModuleName),
            "engine().has_module(civet) after clear", __LINE__);
    }

    // 全部用例入口：返回失败断言数
    int run_all() {
        test_report_command();
        test_command66();
        test_parse_report();
        test_battery();
        test_epsilon();
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
        std::printf("civet_device_test: all cases passed\n");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "civet_device_test: %d assertion(s) failed\n", failed);
    return EXIT_FAILURE;
}
