/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 规则图求值链路与启用级联的回归测试：
//   1) 插件图默认输出的上游子图求值 → 总图插件输出节点 → 通道节点；
//   2) 启动/停止通道、启用/停用图输出时，依赖集合（子规则与事件）的级联启用；
//   3) 事件侧运行期启用集合对映射 enabled 的覆盖。
// 全部用例在临时目录内构造规则图与配置，不读写仓库里的 config/。

#include "AppConfig.h"
#include "EventMapping.h"
#include "EventRuntime.h"
#include "ModuleManager.h"
#include "RuleGraph.h"
#include "RuleManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
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
    void check_true(const char* case_name, bool expr, const char* text, int line) {
        if (expr) {
            return;
        }
        fail(case_name, text, line);
    }

    // 字符串断言
    void check_string(const char* case_name, const std::string& expected, const std::string& actual,
        const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        const std::string detail = std::string(expr) + " expected \"" + expected + "\" actual \""
            + actual + "\"";
        fail(case_name, detail.c_str(), line);
    }

    // 写文本文件（父目录须已存在）
    bool write_text(const std::string& path, const std::string& text) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }
        file << text;
        return true;
    }

    // 字符串列表中是否含某值
    bool contains(const std::vector<std::string>& values, const std::string& target) {
        for (const auto& value : values) {
            if (value == target) {
                return true;
            }
        }
        return false;
    }

    // 插件图样例：hp + damage → 默认输出（与 config/rules/plugin_buckshot.json 同形状）
    const char* PLUGIN_GRAPH_JSON = R"({
    "graph": {
        "plugin": "probe",
        "nextNodeId": 5,
        "nextEdgeId": 4,
        "nodes": [
            { "id": 1, "type": "module", "name": "生命值", "label": "生命值", "valueId": "hp" },
            { "id": 2, "type": "module", "name": "本回合伤害", "label": "本回合伤害", "valueId": "damage" },
            { "id": 3, "type": "rule", "ruleName": "hp_damage", "name": "hp_damage",
              "expression": "{}+{}", "mode": 1, "enabled": true },
            { "id": 4, "type": "defaultOutput", "name": "默认输出", "pluginId": "probe",
              "references": [ "top" ] }
        ],
        "edges": [
            { "id": 1, "from": 1, "to": 3, "port": 0 },
            { "id": 2, "from": 2, "to": 3, "port": 1 },
            { "id": 3, "from": 3, "to": 4, "port": 0 }
        ]
    },
    "kind": "pluginRuleGraph",
    "plugin": "probe",
    "rules": {
        "hp_damage": {
            "enabled": true,
            "mode": 1,
            "parents": [],
            "valuePattern": "{id:hp(生命值)}+{id:damage(本回合伤害)}"
        }
    },
    "version": "1.0"
})";

    // 总图样例：插件输出节点 → 通道 A
    const char* TOP_GRAPH_JSON = R"({
    "graph": {
        "plugin": "",
        "nextNodeId": 3,
        "nextEdgeId": 2,
        "nodes": [
            { "id": 1, "type": "pluginOutput", "name": "探针输出", "pluginId": "probe",
              "references": [ "plugin:probe" ] },
            { "id": 2, "type": "channel", "name": "通道 A", "channel": "A" }
        ],
        "edges": [
            { "id": 1, "from": 1, "to": 2, "port": 0 }
        ]
    },
    "rules": {},
    "version": "1.0"
})";

} // namespace

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_STRING(case_name, expected, actual) check_string((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // 沙箱放在测试工作目录下（系统 TEMP 在部分环境里不可用），用例结束即递归删除
    QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_rule_graph_XXXXXX"));
    if (!temp.isValid()) {
        std::fprintf(stderr, "FAIL: 临时目录创建失败\n");
        return 2;
    }
    const std::string root = temp.path().toStdString();
    const std::string config_dir = root + "/config";
    const std::string rules_dir = root + "/rules";
    std::filesystem::create_directories(config_dir);
    std::filesystem::create_directories(rules_dir);

    // 样例规则图：总图（插件输出 → 通道 A）与插件图（hp + damage → 默认输出）
    if (!write_text(rules_dir + "/rules.json", TOP_GRAPH_JSON)
        || !write_text(rules_dir + "/plugin_probe.json", PLUGIN_GRAPH_JSON)) {
        std::fprintf(stderr, "FAIL: 样例文件写入失败\n");
        return 2;
    }

    if (!AppConfig::instance().initialize(config_dir)) {
        std::fprintf(stderr, "FAIL: 配置初始化失败\n");
        return 2;
    }

    // 规则目录改为临时沙箱（写入运行期配置，不落盘）：内置默认配置的 rule.path 指向仓库 config/，
    // 随后重新扫描目录并载入总图所在的规则文件
    AppConfig::instance().set_value<std::string>("rule.path", rules_dir);
    AppConfig::instance().set_value<std::string>("rule.key", "rule");
    RuleManager::instance().init();
    RuleManager::instance().load_rule_file("rules.json");

    // 侧车数值：插件标识 probe 提供 hp / damage（运行期注册，不落盘）
    std::vector<ModuleValue> values;
    values.emplace_back("hp", "生命值", QueryPeriod::SECOND, "hp");
    values.emplace_back("damage", "本回合伤害", QueryPeriod::SECOND, "damage");
    CHECK_TRUE("module-register", ModuleManager::instance().register_module_values("probe", values, { "A" }));

    // 1) 依赖集合：总图查询并入插件图一层
    RuleGraph top_graph;
    top_graph.load(QStringLiteral("rules.json"));
    CHECK_TRUE("top-graph-loaded", !top_graph.nodes().empty());
    const GraphDependency dependency = RuleManager::instance().get_graph_dependencies(top_graph);
    CHECK_TRUE("dependency-rule", contains(dependency.rules, "hp_damage"));
    CHECK_TRUE("dependency-value-hp", contains(dependency.value_ids, "hp"));
    CHECK_TRUE("dependency-value-damage", contains(dependency.value_ids, "damage"));
    CHECK_TRUE("dependency-channel", contains(dependency.channels, "A"));
    CHECK_TRUE("dependency-plugin", contains(dependency.plugins, "probe"));

    // 1b) 事件侧：插件声明的事件进入依赖集合（纯数据结构，不依赖插件加载）
    GraphDependency event_dependency;
    event_dependency.plugins = { "probe" };
    std::map<std::string, std::vector<std::string>> plugin_events;
    plugin_events["probe"] = { "hit", "threshold" };
    RuleManager::fill_dependency_events(plugin_events, event_dependency);
    CHECK_INT("dependency-event-count", 2, static_cast<int>(event_dependency.events.size()));
    CHECK_STRING("dependency-event-first", "probe/hit", event_dependency.events[0]);
    CHECK_STRING("dependency-event-second", "probe/threshold", event_dependency.events[1]);

    // 2) 启动通道 A：依赖集合中的子规则视为启用
    int command_count = 0;
    QJsonObject last_command;
    QObject::connect(&RuleManager::instance(), &RuleManager::rule_command_ready,
        [&command_count, &last_command](const QJsonObject& cmd) {
            ++command_count;
            last_command = cmd;
        });

    RuleManager::instance().set_channel_enabled("A", true);
    const std::vector<std::string> cascade_rules = RuleManager::instance().get_cascade_enabled_rules();
    CHECK_INT("cascade-rule-count", 1, static_cast<int>(cascade_rules.size()));
    CHECK_TRUE("cascade-rule-member", contains(cascade_rules, "hp_damage"));
    CHECK_TRUE("cascade-rule-effective", RuleManager::instance().is_rule_effectively_enabled("hp_damage"));
    CHECK_INT("cascade-event-empty", 0,
        static_cast<int>(RuleManager::instance().get_cascade_enabled_events().size()));

    // 3) 多图求值：模块数值变化 → 插件图 40+7 → 总图插件输出 → 通道 A
    ModuleManager::instance().set_value("probe", "hp", 40);
    CHECK_TRUE("no-output-when-incomplete", !RuleManager::instance().get_plugin_graph_output("probe").has_value());
    ModuleManager::instance().set_value("probe", "damage", 7);

    const std::optional<int> plugin_output = RuleManager::instance().get_plugin_graph_output("probe");
    CHECK_TRUE("plugin-graph-output-has-value", plugin_output.has_value());
    CHECK_INT("plugin-graph-output", 47, plugin_output.value_or(-1));
    const std::optional<int> channel_output = RuleManager::instance().get_graph_channel_output("A");
    CHECK_TRUE("channel-output-has-value", channel_output.has_value());
    CHECK_INT("channel-output", 47, channel_output.value_or(-1));
    CHECK_INT("channel-command-count", 1, command_count);
    CHECK_STRING("channel-command-cmd", "send_strength",
        last_command.value(QStringLiteral("cmd")).toString().toStdString());
    CHECK_STRING("channel-command-channel", "A",
        last_command.value(QStringLiteral("channel")).toString().toStdString());
    CHECK_INT("channel-command-value", 47, last_command.value(QStringLiteral("value")).toInt());

    // 4) 停止通道 A：级联解除，求值继续但不再下发
    RuleManager::instance().set_channel_enabled("A", false);
    CHECK_INT("cascade-cleared", 0,
        static_cast<int>(RuleManager::instance().get_cascade_enabled_rules().size()));
    CHECK_TRUE("cascade-rule-not-effective",
        !RuleManager::instance().is_rule_effectively_enabled("hp_damage"));
    ModuleManager::instance().set_value("probe", "hp", 50);
    CHECK_INT("channel-command-count-stopped", 1, command_count);
    const std::optional<int> stopped_output = RuleManager::instance().get_graph_channel_output("A");
    CHECK_TRUE("channel-output-after-stop-has-value", stopped_output.has_value());
    CHECK_INT("channel-output-after-stop", 57, stopped_output.value_or(-1));

    // 5) 规则图输出的启停（同样带级联）
    CHECK_TRUE("graph-output-disabled-default", !RuleManager::instance().is_graph_output_enabled("plugin:probe"));
    CHECK_TRUE("graph-output-enable", RuleManager::instance().set_graph_output_enabled("plugin:probe", true));
    CHECK_TRUE("graph-output-enabled", RuleManager::instance().is_graph_output_enabled("plugin:probe"));
    CHECK_TRUE("graph-output-cascade-rule",
        contains(RuleManager::instance().get_cascade_enabled_rules(), "hp_damage"));
    CHECK_TRUE("graph-output-disable", RuleManager::instance().set_graph_output_enabled("plugin:probe", false));
    CHECK_TRUE("graph-output-disabled", !RuleManager::instance().is_graph_output_enabled("plugin:probe"));
    CHECK_INT("graph-output-cascade-cleared", 0,
        static_cast<int>(RuleManager::instance().get_cascade_enabled_rules().size()));
    CHECK_TRUE("graph-output-unknown-ref", !RuleManager::instance().set_graph_output_enabled("", true));

    // 6) 事件侧：运行期启用集合覆盖映射的 enabled，且不写回配置
    std::map<std::string, EventConfig> configs;
    EventConfig hit_config;
    hit_config.enabled = false;
    configs["hit"] = hit_config;
    EventConfig quiet_config;
    quiet_config.enabled = false;
    configs["quiet"] = quiet_config;
    const std::set<std::string> whitelist = { "probe/hit" };
    apply_cascade_enabled("probe", whitelist, configs);
    CHECK_TRUE("event-mapping-override", configs["hit"].enabled);
    CHECK_TRUE("event-mapping-untouched", !configs["quiet"].enabled);

    EventRuntime runtime;
    runtime.set_cascade_events(whitelist);
    CHECK_INT("event-runtime-whitelist", 1, static_cast<int>(runtime.cascade_events().size()));
    CHECK_INT("event-runtime-whitelist-member", 1,
        static_cast<int>(runtime.cascade_events().count("probe/hit")));
    runtime.set_cascade_events({});
    CHECK_INT("event-runtime-cleared", 0, static_cast<int>(runtime.cascade_events().size()));

    if (failures == 0) {
        std::printf("rule_graph_eval_test: 全部用例通过\n");
        return 0;
    }
    std::fprintf(stderr, "rule_graph_eval_test: %d 处断言失败\n", failures);
    return 1;
}
