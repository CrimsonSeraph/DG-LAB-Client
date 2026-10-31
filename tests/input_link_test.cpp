/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 输入设备多实例测试：实例模块名生成（MAC 后四位）与同后四位消歧，
// 以及「注册 → 事件声明出现 → 断开注销 → 事件声明消失 → 再注册重新出现」的全链路。
// 配置在临时目录内构造，不读写仓库里的 config/

#include "AppConfig.h"
#include "BleDeviceManager.h"
#include "DeviceValueBridge.h"
#include "EventMapping.h"
#include "EventRuntime.h"
#include "ModuleManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QTemporaryDir>

#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

namespace {

    int failures = 0; ///< 失败断言数

    // 记录一次断言失败
    void fail(const char* case_name, const char* detail, int line) {
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", case_name, line, detail);
        ++failures;
    }

    // 整型断言：形参用 long long 与 %lld 直配，避免 qsizetype / size_t 传入时的收窄告警
    void check_int(const char* case_name, long long expected, long long actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %lld actual %lld", expr, expected, actual);
        fail(case_name, detail, line);
    }

    // 布尔断言
    void check_true(const char* case_name, bool actual, const char* expr, int line) {
        if (actual) {
            return;
        }
        fail(case_name, expr, line);
    }

    // 文本断言
    void check_text(const char* case_name, const QString& expected, const QString& actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[512] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %s actual %s", expr, expected.toUtf8().constData(),
            actual.toUtf8().constData());
        fail(case_name, detail, line);
    }

    // 实例模块名：MAC 后四位（去分隔符、转大写）与显示名
    void test_instance_names() {
        const QString mac = QStringLiteral("D3:3B:5A:FA:43:0C");
        check_text("suffix", QStringLiteral("430C"), InputModuleNames::mac_suffix(mac), "mac_suffix(D3:3B:5A:FA:43:0C)",
            __LINE__);
        check_text("suffix-lower", QStringLiteral("430C"), InputModuleNames::mac_suffix(QStringLiteral("d3:3b:5a:fa:43:0c")),
            "mac_suffix(lowercase)", __LINE__);
        check_text("name-paw", QStringLiteral("paw_430C"), InputModuleNames::base_name(QStringLiteral("paw"), mac),
            "base_name(paw)", __LINE__);
        check_text("name-civet", QStringLiteral("civet_430C"), InputModuleNames::base_name(QStringLiteral("civet"), mac),
            "base_name(civet)", __LINE__);
        check_text("display-paw", QStringLiteral("爪印 430C"), InputModuleNames::display_name(QStringLiteral("爪印"), mac),
            "display_name(爪印)", __LINE__);
        check_text("display-empty-label", QStringLiteral("430C"), InputModuleNames::display_name(QString(), mac),
            "display_name(empty label)", __LINE__);
        check_text("suffix-empty", QStringLiteral("paw"), InputModuleNames::base_name(QStringLiteral("paw"), QString()),
            "base_name(paw, empty mac)", __LINE__);
    }

    // 同后四位冲突消歧：追加 _2 / _3，释放后名字可被复用
    void test_name_conflicts() {
        InputModuleNames table;
        int ordinal = 0;
        const QString first = table.acquire(QStringLiteral("paw"), QStringLiteral("AA:BB:CC:DD:43:0C"), &ordinal);
        check_text("conflict-first", QStringLiteral("paw_430C"), first, "acquire(first)", __LINE__);
        check_int("conflict-first-ordinal", 1, ordinal, "ordinal(first)", __LINE__);
        const QString second = table.acquire(QStringLiteral("paw"), QStringLiteral("11:22:33:44:43:0C"), &ordinal);
        check_text("conflict-second", QStringLiteral("paw_430C_2"), second, "acquire(second)", __LINE__);
        check_int("conflict-second-ordinal", 2, ordinal, "ordinal(second)", __LINE__);
        const QString third = table.acquire(QStringLiteral("paw"), QStringLiteral("55:66:77:88:43:0C"), &ordinal);
        check_text("conflict-third", QStringLiteral("paw_430C_3"), third, "acquire(third)", __LINE__);
        check_int("conflict-count", 3, table.count(), "count()", __LINE__);
        // 释放中间那个名字后，下一个同后四位设备复用它
        table.release(second);
        check_int("conflict-after-release", 2, table.count(), "count() after release", __LINE__);
        const QString reused = table.acquire(QStringLiteral("paw"), QStringLiteral("99:88:77:66:43:0C"), &ordinal);
        check_text("conflict-reuse", QStringLiteral("paw_430C_2"), reused, "acquire after release", __LINE__);
        // 不同后四位不冲突
        const QString other = table.acquire(QStringLiteral("paw"), QStringLiteral("AA:BB:CC:DD:11:22"), &ordinal);
        check_text("conflict-other-suffix", QStringLiteral("paw_1122"), other, "acquire(other suffix)", __LINE__);
        check_int("conflict-other-ordinal", 1, ordinal, "ordinal(other suffix)", __LINE__);
        // 灵猫与爪印前缀不同，即使后四位相同也互不影响
        const QString civet = table.acquire(QStringLiteral("civet"), QStringLiteral("AA:BB:CC:DD:43:0C"), &ordinal);
        check_text("conflict-other-prefix", QStringLiteral("civet_430C"), civet, "acquire(civet)", __LINE__);
    }

    // 注册 / 断开注销：模块数值、内置事件声明与引擎运行期状态三者同步增删
    void test_instance_registration() {
        QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_input_link_XXXXXX"));
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
        // 只为单个实例的事件启用：级联集合按「模块标识/事件 ID」键控，两台设备互不干扰
        std::set<std::string> cascade;
        cascade.insert("paw_430C/button_pressed");

        EventRuntime runtime;
        runtime.initialize();
        // 启用集合必须先于实例注册到位：register_builtin_events() 会重载引擎，重载后重建的
        // 运行期状态才带上 enabled，且重载会清掉各模块的前值基线
        runtime.set_cascade_events(cascade);

        DeviceValueBridge first(DeviceValueBridge::DeviceKind::PawPrints, "paw_430C", "爪印 430C");
        first.attach_event_runtime(&runtime);
        first.initialize();
        check_text("register-module-name", QStringLiteral("paw_430C"), QString::fromStdString(first.module_name()),
            "module_name()", __LINE__);
        check_true("register-values", ModuleManager::instance().get_module("paw_430C") != nullptr,
            "get_module(paw_430C)", __LINE__);
        check_true("register-engine", runtime.engine().has_module("paw_430C"), "engine.has_module(paw_430C)", __LINE__);
        check_int("register-builtin-count", 1, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size()", __LINE__);
        // 数值语义不变：首次写入只登记前值（桥接初始化时的基线已写 0），第二次写入触发本实例的事件
        ModuleManager::instance().set_value("paw_430C", "button", 0);
        check_int("register-first-write", 0, static_cast<int>(runtime.engine().recent_triggers(50).size()),
            "triggers after first write", __LINE__);
        ModuleManager::instance().set_value("paw_430C", "button", 1);
        check_int("register-trigger", 1, static_cast<int>(runtime.engine().recent_triggers(50).size()),
            "triggers after press", __LINE__);

        // 第二台同型号：同后四位消歧后的模块名并存，两份声明互不覆盖
        DeviceValueBridge second(DeviceValueBridge::DeviceKind::PawPrints, "paw_430C_2", "爪印 430C");
        second.attach_event_runtime(&runtime);
        second.initialize();
        check_true("multi-values-both", ModuleManager::instance().get_module("paw_430C_2") != nullptr,
            "get_module(paw_430C_2)", __LINE__);
        check_true("multi-engine-both", runtime.engine().has_module("paw_430C_2"),
            "engine.has_module(paw_430C_2)", __LINE__);
        check_int("multi-builtin-count", 2, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size() after second", __LINE__);
        // 灵猫实例按自己的前缀注册，与爪印互不影响
        DeviceValueBridge civet(DeviceValueBridge::DeviceKind::Civet, "civet_430C", "灵猫 430C");
        civet.attach_event_runtime(&runtime);
        civet.initialize();
        check_true("civet-module", ModuleManager::instance().get_module("civet_430C") != nullptr,
            "get_module(civet_430C)", __LINE__);
        check_int("civet-builtin-count", 3, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size() after civet", __LINE__);

        // 断开第二台：数值模块、内置声明与引擎状态一起消失，第一台不受影响
        second.shutdown();
        check_true("unregister-values", ModuleManager::instance().get_module("paw_430C_2") == nullptr,
            "get_module(paw_430C_2) after shutdown", __LINE__);
        check_true("unregister-engine", !runtime.engine().has_module("paw_430C_2"),
            "engine.has_module(paw_430C_2) after shutdown", __LINE__);
        check_int("unregister-builtin-count", 2, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size() after shutdown", __LINE__);
        check_true("unregister-builtin-gone", runtime.builtin_events().count("paw_430C_2") == 0,
            "builtin_events().count(paw_430C_2)", __LINE__);
        check_true("unregister-keeps-other", runtime.engine().has_module("paw_430C"),
            "engine.has_module(paw_430C) after other shutdown", __LINE__);
        // 重复注销无副作用
        second.shutdown();

        // 再连上：声明与模块重新出现
        DeviceValueBridge again(DeviceValueBridge::DeviceKind::PawPrints, "paw_430C_2", "爪印 430C");
        again.attach_event_runtime(&runtime);
        again.initialize();
        check_true("reconnect-engine", runtime.engine().has_module("paw_430C_2"),
            "engine.has_module(paw_430C_2) after reconnect", __LINE__);
        check_int("reconnect-builtin-count", 3, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size() after reconnect", __LINE__);

        again.shutdown();
        civet.shutdown();
        first.shutdown();
        check_int("final-builtin-count", 0, static_cast<int>(runtime.builtin_events().size()),
            "builtin_events().size() at end", __LINE__);
        check_true("final-engine-empty", !runtime.engine().has_module("paw_430C"),
            "engine.has_module(paw_430C) at end", __LINE__);
    }

    // 旧配置迁移：单实例时代的 app.plugins.paw 搬到第一台连上的实例，只搬一次
    void test_legacy_mapping_migration() {
        QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_input_migration_XXXXXX"));
        if (!temp.isValid()) {
            fail("migration-temp-dir", "临时目录创建失败", __LINE__);
            return;
        }
        const std::string config_dir = temp.path().toStdString() + "/config";
        std::filesystem::create_directories(config_dir);
        if (!AppConfig::instance().initialize(config_dir)) {
            fail("migration-config-init", "配置初始化失败", __LINE__);
            return;
        }
        nlohmann::json legacy = nlohmann::json::object();
        legacy["channel"] = "B";
        legacy["events"] = nlohmann::json::object();
        legacy["events"]["button_pressed"] = nlohmann::json::object();
        legacy["events"]["button_pressed"]["dataType"] = "strength";
        AppConfig::instance().set_value_with_name<nlohmann::json>("app.plugins.paw", legacy, "user");
        AppConfig::instance().save_all();

        EventRuntime runtime;
        runtime.initialize();
        DeviceValueBridge first(DeviceValueBridge::DeviceKind::PawPrints, "paw_4C0F", "爪印 4C0F");
        first.attach_event_runtime(&runtime);
        first.initialize();
        const nlohmann::json migrated = AppConfig::instance().get_value<nlohmann::json>(
            "app.plugins.paw_4C0F", nlohmann::json::object());
        check_true("migration-copied", migrated.is_object() && migrated.contains("events"),
            "app.plugins.paw_4C0F.events", __LINE__);
        check_text("migration-channel", QStringLiteral("B"),
            QString::fromStdString(migrated.value("channel", std::string())), "migrated channel", __LINE__);
        check_text("migration-marker", QStringLiteral("paw_4C0F"),
            QString::fromStdString(AppConfig::instance().get_value<std::string>(
                "app.plugins.paw.module_migrated_to", std::string())),
            "app.plugins.paw.module_migrated_to", __LINE__);

        // 第二台：旧映射只搬一次，不会被第二台抢走
        DeviceValueBridge second(DeviceValueBridge::DeviceKind::PawPrints, "paw_4C10", "爪印 4C10");
        second.attach_event_runtime(&runtime);
        second.initialize();
        const nlohmann::json second_mapping = AppConfig::instance().get_value<nlohmann::json>(
            "app.plugins.paw_4C10", nlohmann::json::object());
        check_true("migration-once", second_mapping.is_object() && second_mapping.empty(),
            "app.plugins.paw_4C10 empty", __LINE__);

        second.shutdown();
        first.shutdown();
    }

} // namespace

// 全部用例入口：返回失败断言数
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    test_instance_names();
    test_name_conflicts();
    test_instance_registration();
    test_legacy_mapping_migration();
    if (failures == 0) {
        std::printf("input_link_test: all cases passed\n");
    }
    return failures == 0 ? 0 : 1;
}
