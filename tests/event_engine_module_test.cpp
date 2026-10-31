/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 事件引擎模块移除的回归测试：注册模块并触发 → remove_module → 再发生数值变化不触发、无残留声明
// 通道上限在临时目录内构造，不读写仓库里的 config/

#include "AppConfig.h"
#include "EventEngine.h"
#include "EventMapping.h"
#include "ModuleManager.h"
#include "ModuleValue.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

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
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected true actual false", expr);
        fail(case_name, detail, line);
    }

    // 构造引用数值的事件声明（任意变化即触发）
    EventDefinition make_definition(const std::string& id, const std::string& value_id) {
        EventDefinition definition;
        definition.id = id;
        definition.label = id;
        definition.trigger.value_id = value_id;
        definition.trigger.compare = EventCompare::ANY;
        return definition;
    }

    // 构造单一子选项的映射：SET + 百分比输出到指定通道
    EventConfig make_config(const std::string& channel, int percent) {
        EventConfig config;
        config.enabled = true;
        config.frames = DEFAULT_EVENT_FRAMES;
        OptionOutput output;
        output.channel = channel;
        output.operation = EventOperation::SET;
        output.mode = OptionMode::PERCENT;
        output.percent = percent;
        config.options["default"] = output;
        return config;
    }

} // namespace

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // 沙箱放在测试工作目录下（系统 TEMP 在部分环境里不可用），用例结束即递归删除
    QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_event_engine_XXXXXX"));
    if (!temp.isValid()) {
        std::fprintf(stderr, "FAIL: 临时目录创建失败\n");
        return 2;
    }
    const std::string config_dir = temp.path().toStdString() + "/config";
    std::filesystem::create_directories(config_dir);
    if (!AppConfig::instance().initialize(config_dir)) {
        std::fprintf(stderr, "FAIL: 配置初始化失败\n");
        return 2;
    }

    // 通道上限固定为 200 / 150：百分比输出可直接换算成强度（50% → 100，25% → 50）
    EventChannelConfig channel_config;
    channel_config.limit = 200;
    channel_config.safety = 150;
    write_channel_config("A", channel_config);
    write_channel_config("B", channel_config);

    // 两个模块各挂一个数值：probe 输出 A，other 输出 B
    std::vector<ModuleValue> values;
    values.emplace_back("hp", "生命值", QueryPeriod::SECOND, "hp");
    CHECK_TRUE("module-register-probe",
        ModuleManager::instance().register_module_values("probe", values, { "A" }));
    CHECK_TRUE("module-register-other",
        ModuleManager::instance().register_module_values("other", values, { "B" }));

    EventEngine engine;
    const std::vector<EventDefinition> probe_definitions = { make_definition("hit", "hp") };
    std::map<std::string, EventConfig> probe_configs;
    probe_configs["hit"] = make_config("A", 50);
    engine.configure("probe", probe_definitions, probe_configs);
    const std::vector<EventDefinition> other_definitions = { make_definition("hit", "hp") };
    std::map<std::string, EventConfig> other_configs;
    other_configs["hit"] = make_config("B", 25);
    engine.configure("other", other_definitions, other_configs);
    CHECK_TRUE("probe-configured", engine.has_module("probe"));
    CHECK_TRUE("other-configured", engine.has_module("other"));

    // 1) 首次写入只登记前值：不判定触发、不产出强度
    engine.on_value_changed("probe", "hp", 10);
    engine.advance_frame(1000);
    CHECK_INT("first-value-channel-a", 0, engine.channel_output("A"));
    CHECK_INT("first-value-trigger-count", 0, static_cast<int>(engine.recent_triggers(200).size()));

    // 2) 值变化触发：50% × 200 = 100，并登记一条流水
    engine.on_value_changed("probe", "hp", 11);
    CHECK_INT("trigger-recorded", 1, static_cast<int>(engine.recent_triggers(200).size()));
    engine.advance_frame(1100);
    CHECK_INT("channel-a-after-trigger", 100, engine.channel_output("A"));

    // 另一个模块在 B 输出 25% × 200 = 50
    engine.on_value_changed("other", "hp", 5);
    engine.on_value_changed("other", "hp", 6);
    engine.advance_frame(1200);
    CHECK_INT("channel-b-after-trigger", 50, engine.channel_output("B"));

    // 3) 移除 probe：声明与运行期状态一并消失，other 的运行期状态不受影响
    engine.remove_module("probe");
    CHECK_TRUE("probe-removed", !engine.has_module("probe"));
    CHECK_TRUE("other-kept", engine.has_module("other"));
    engine.advance_frame(1300);
    CHECK_INT("channel-a-cleared-after-remove", 0, engine.channel_output("A"));
    CHECK_INT("channel-b-kept-after-remove", 50, engine.channel_output("B"));

    // 4) 移除后再发生数值变化：不触发、不重建任何状态
    const long long triggers_before = static_cast<long long>(engine.recent_triggers(200).size());
    CHECK_INT("trigger-count-before-value-change", 2, triggers_before);
    engine.on_value_changed("probe", "hp", 12);
    engine.on_value_changed("probe", "hp", 13);
    engine.advance_frame(1400);
    CHECK_INT("no-trigger-after-remove", triggers_before,
        static_cast<long long>(engine.recent_triggers(200).size()));
    CHECK_INT("channel-a-still-zero", 0, engine.channel_output("A"));
    CHECK_TRUE("probe-state-not-rebuilt", !engine.has_module("probe"));
    CHECK_INT("channel-b-unaffected", 50, engine.channel_output("B"));

    // 5) 空标识与未注册模块的移除是无操作
    engine.remove_module("");
    engine.remove_module("ghost");
    CHECK_TRUE("probe-absent-after-noop-remove", !engine.has_module("probe"));
    CHECK_TRUE("other-alive-after-noop-remove", engine.has_module("other"));

    // 6) 插件重新加载后重新载入声明：触发恢复，移除没有污染引擎
    engine.configure("probe", probe_definitions, probe_configs);
    CHECK_TRUE("probe-reconfigured", engine.has_module("probe"));
    engine.on_value_changed("probe", "hp", 20);
    engine.on_value_changed("probe", "hp", 21);
    engine.advance_frame(1500);
    CHECK_INT("channel-a-after-reconfigure", 100, engine.channel_output("A"));

    if (failures == 0) {
        std::printf("event_engine_module_test: 全部用例通过\n");
        return EXIT_SUCCESS;
    }
    std::printf("event_engine_module_test: %d 处断言失败\n", failures);
    return EXIT_FAILURE;
}
