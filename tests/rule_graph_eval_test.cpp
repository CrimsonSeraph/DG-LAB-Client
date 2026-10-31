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

#include <nlohmann/json.hpp>

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

    // 读取 user 层的原始 JSON 子树（缺失返回空对象）
    nlohmann::json raw_subtree(const std::string& key) {
        return AppConfig::instance().get_value<nlohmann::json>(key, nlohmann::json::object());
    }

    // 插件图样例：hp + damage → 默认输出（与 config/rules/plugin_<插件标识>.json 同形状）
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

    // 7) 全局通道上限：键为 app.channels.<通道>，与插件无关
    EventChannelConfig channel_a;
    channel_a.limit = 240;
    channel_a.safety = 180;
    write_channel_config("A", channel_a);
    CHECK_INT("channel-global-limit", 240, read_channel_config("A").limit);
    CHECK_INT("channel-global-safety", 180, read_channel_config("A").safety);
    CHECK_INT("channel-global-accessor-limit", 240, channel_limit("a"));
    CHECK_INT("channel-global-accessor-safety", 180, channel_safety("A"));
    CHECK_INT("channel-invalid-default-limit", 200, read_channel_config("C").limit);
    CHECK_INT("channel-invalid-write-skipped", 0,
        static_cast<int>(raw_subtree("app.channels.C").size()));

    // 7b) 旧键迁移：全局键缺失时取 app.plugins.<模块>.channels.<通道> 的首个存在值并写回全局键
    AppConfig::instance().set_value<nlohmann::json>("app.plugins.legacy.channels.B",
        nlohmann::json{ { "limit", 111 }, { "safety", 99 } });
    const EventChannelConfig migrated = read_channel_config("B");
    CHECK_INT("channel-migrate-limit", 111, migrated.limit);
    CHECK_INT("channel-migrate-safety", 99, migrated.safety);
    CHECK_INT("channel-migrate-persisted", 111, raw_subtree("app.channels.B").value("limit", 0));

    // 7c) 迁移只发生一次：全局键已存在后不再读旧键
    AppConfig::instance().set_value<nlohmann::json>("app.plugins.legacy.channels.B",
        nlohmann::json{ { "limit", 222 }, { "safety", 222 } });
    CHECK_INT("channel-migrate-once", 111, read_channel_config("B").limit);
    CHECK_INT("channel-migrate-once-safety", 99, read_channel_config("B").safety);

    // 8) 事件输出数据类型：三个取值可读写，非法与缺失回落 strength
    EventDefinition typed_definition;
    typed_definition.id = "typed";
    EventConfig typed_config;
    typed_config.dataType = EventDataType::WAVE;
    write_event_config("probe", "typed", typed_config);
    CHECK_STRING("data-type-wave", "wave",
        event_data_type_to_string(read_event_config("probe", typed_definition).dataType));
    CHECK_STRING("data-type-wave-disk", "wave",
        raw_subtree("app.plugins.probe.events.typed").value("dataType", std::string()));

    typed_config.dataType = EventDataType::PERCENT;
    write_event_config("probe", "typed", typed_config);
    CHECK_STRING("data-type-percent", "percent",
        event_data_type_to_string(read_event_config("probe", typed_definition).dataType));

    typed_config.dataType = EventDataType::STRENGTH;
    write_event_config("probe", "typed", typed_config);
    CHECK_STRING("data-type-strength", "strength",
        event_data_type_to_string(read_event_config("probe", typed_definition).dataType));

    AppConfig::instance().set_value<std::string>("app.plugins.probe.events.typed.dataType", "bogus");
    CHECK_STRING("data-type-invalid-fallback", "strength",
        event_data_type_to_string(read_event_config("probe", typed_definition).dataType));

    EventDefinition missing_definition;
    missing_definition.id = "no_data_type";
    CHECK_STRING("data-type-missing-default", "strength",
        event_data_type_to_string(read_event_config("probe", missing_definition).dataType));

    // 9) 事件启用不落盘：写事件配置不产生 enabled 字段
    const nlohmann::json typed_raw = raw_subtree("app.plugins.probe.events.typed");
    CHECK_TRUE("event-enabled-not-persisted", typed_raw.find("enabled") == typed_raw.end());

    // 10) 通道节点改造：目标设备解析（跟随默认 / 指定 MAC / 离线 MAC）
    const QStringList online = { QStringLiteral("D3:3B:5A:FA:43:0C"),
        QStringLiteral("AA:BB:CC:DD:EE:01") };
    const QString default_device = QStringLiteral("d3:3b:5a:fa:43:0c");

    // 跟随默认设备：解析到当前默认设备（大小写不敏感）
    const ChannelTargetResolution follow = rule_graph_resolve_channel_target(
        QString(), default_device, online);
    CHECK_TRUE("target-follow-ready", follow.status == ChannelTargetStatus::Ready);
    CHECK_STRING("target-follow-mac", "D3:3B:5A:FA:43:0C", follow.mac.toStdString());

    // 指定在线 MAC：解析到该设备
    const ChannelTargetResolution pinned = rule_graph_resolve_channel_target(
        QStringLiteral("aa:bb:cc:dd:ee:01"), default_device, online);
    CHECK_TRUE("target-pinned-ready", pinned.status == ChannelTargetStatus::Ready);
    CHECK_STRING("target-pinned-mac", "AA:BB:CC:DD:EE:01", pinned.mac.toStdString());

    // 指定离线 MAC：Offline 且不产出目标设备
    const ChannelTargetResolution offline = rule_graph_resolve_channel_target(
        QStringLiteral("11:22:33:44:55:66"), default_device, online);
    CHECK_TRUE("target-offline-status", offline.status == ChannelTargetStatus::Offline);
    CHECK_TRUE("target-offline-mac-empty", offline.mac.isEmpty());

    // 跟随默认但没有默认设备 / 指定 MAC 且一台设备都没在线：都判为不可下发
    const ChannelTargetResolution no_default = rule_graph_resolve_channel_target(
        QString(), QString(), online);
    CHECK_TRUE("target-no-default", no_default.status == ChannelTargetStatus::NoDefault);
    const ChannelTargetResolution none_online = rule_graph_resolve_channel_target(
        QStringLiteral("D3:3B:5A:FA:43:0C"), QString(), QStringList());
    CHECK_TRUE("target-none-online", none_online.status == ChannelTargetStatus::Offline);

    // 11) 唯一入边：同一设备同一通道只保留一条入边，不同设备互不影响
    RuleGraph scope_graph;
    scope_graph.set_default_device(default_device);
    GraphNode source_a;
    source_a.type = GraphNodeType::Module;
    source_a.value_id = QStringLiteral("hp");
    source_a.name = QStringLiteral("生命值");
    const int source_a_id = scope_graph.add_node(source_a);
    GraphNode source_b = source_a;
    source_b.value_id = QStringLiteral("damage");
    source_b.name = QStringLiteral("伤害");
    const int source_b_id = scope_graph.add_node(source_b);

    GraphNode channel_follow;
    channel_follow.type = GraphNodeType::Channel;
    channel_follow.channel = QStringLiteral("A");
    channel_follow.name = QStringLiteral("通道 A");
    const int channel_follow_id = scope_graph.add_node(channel_follow);
    // 指定「当前默认设备」的同一 MAC：与「跟随默认设备」落进同一作用域
    GraphNode channel_same = channel_follow;
    channel_same.target_device = QStringLiteral("D3:3B:5A:FA:43:0C");
    const int channel_same_id = scope_graph.add_node(channel_same);
    // 另一台设备：不同作用域，互不影响
    GraphNode channel_pinned = channel_follow;
    channel_pinned.target_device = QStringLiteral("AA:BB:CC:DD:EE:01");
    const int channel_pinned_id = scope_graph.add_node(channel_pinned);
    GraphNode channel_offline = channel_follow;
    channel_offline.target_device = QStringLiteral("11:22:33:44:55:66");
    const int channel_offline_id = scope_graph.add_node(channel_offline);

    // 通道节点是数据汇：一个输入端口，上游可连入
    CHECK_INT("channel-input-count", 1, scope_graph.input_count(channel_follow_id));
    CHECK_STRING("channel-input-label", "输入",
        scope_graph.input_label(channel_follow_id, 0).toStdString());

    // 11a) 同一设备同一通道：第二条入边顶掉第一条（「跟随默认设备」与「指定该默认设备」同一作用域）
    CHECK_TRUE("unique-first-connect", scope_graph.connect_nodes(source_a_id, channel_follow_id, 0) > 0);
    CHECK_INT("unique-first-source", source_a_id, scope_graph.source_at_port(channel_follow_id, 0));
    CHECK_TRUE("unique-second-connect", scope_graph.connect_nodes(source_b_id, channel_same_id, 0) > 0);
    CHECK_INT("unique-first-cleared", -1, scope_graph.source_at_port(channel_follow_id, 0));
    CHECK_INT("unique-second-source", source_b_id, scope_graph.source_at_port(channel_same_id, 0));

    // 11b) 不同设备同名通道互不影响：另一台设备与离线 MAC 的通道 A 各保留自己的入边
    CHECK_TRUE("unique-other-device-connect",
        scope_graph.connect_nodes(source_a_id, channel_pinned_id, 0) > 0);
    CHECK_INT("unique-other-device-kept", source_b_id, scope_graph.source_at_port(channel_same_id, 0));
    CHECK_INT("unique-other-device-source", source_a_id,
        scope_graph.source_at_port(channel_pinned_id, 0));
    CHECK_TRUE("unique-offline-device-connect",
        scope_graph.connect_nodes(source_b_id, channel_offline_id, 0) > 0);
    CHECK_INT("unique-offline-device-kept", source_a_id,
        scope_graph.source_at_port(channel_pinned_id, 0));

    // 11c) 通道 B 与通道 A 不同作用域：同样保留两条
    GraphNode channel_b;
    channel_b.type = GraphNodeType::Channel;
    channel_b.channel = QStringLiteral("B");
    channel_b.name = QStringLiteral("通道 B");
    const int channel_b_id = scope_graph.add_node(channel_b);
    CHECK_TRUE("unique-channel-b-connect", scope_graph.connect_nodes(source_a_id, channel_b_id, 0) > 0);
    CHECK_INT("unique-channel-a-kept", source_b_id, scope_graph.source_at_port(channel_same_id, 0));

    // 11d) 求值按输入端口取值：通道节点把上游值当作待下发的强度
    const RuleGraph::ValueResolver resolver = [](const QString& value_id) -> std::optional<int> {
        return value_id == QStringLiteral("damage") ? std::optional<int>(33) : std::optional<int>();
    };
    const std::optional<int> channel_value = scope_graph.evaluate(channel_same_id, resolver);
    CHECK_TRUE("channel-eval-has-value", channel_value.has_value());
    CHECK_INT("channel-eval-value", 33, channel_value.value_or(-1));

    // 12) 序列化与迁移：targetDevice 落盘；旧文件缺该字段时按跟随默认设备处理
    const nlohmann::json serialized = scope_graph.to_json();
    CHECK_STRING("serialize-target-default", "",
        serialized["nodes"][static_cast<size_t>(channel_follow_id - 1)].value("targetDevice", ""));
    CHECK_STRING("serialize-target-pinned", "AA:BB:CC:DD:EE:01",
        serialized["nodes"][static_cast<size_t>(channel_pinned_id - 1)].value("targetDevice", ""));
    CHECK_STRING("serialize-target-same-device", "D3:3B:5A:FA:43:0C",
        serialized["nodes"][static_cast<size_t>(channel_same_id - 1)].value("targetDevice", ""));

    RuleGraph legacy_graph;
    legacy_graph.from_json(nlohmann::json::parse(R"({
    "nodes": [ { "id": 1, "type": "channel", "name": "通道 A", "channel": "A" } ],
    "edges": []
})"));
    CHECK_INT("legacy-channel-node-count", 1, static_cast<int>(legacy_graph.nodes().size()));
    CHECK_TRUE("legacy-target-follow-default",
        legacy_graph.nodes().front().target_device.isEmpty());
    CHECK_INT("legacy-input-count", 1, legacy_graph.input_count(legacy_graph.nodes().front().id));

    // 13) 批量保存（编辑器「保存全部」的 C++ 路径）：总图在前、插件图按标识排序，
    //     一次调用写完并统一重建引用列表；某一张失败不中断其余
    RuleGraph batch_top;
    batch_top.load(QStringLiteral("rules.json"));
    const long long top_nodes_before = static_cast<long long>(batch_top.nodes().size());
    GraphNode batch_channel;
    batch_channel.type = GraphNodeType::Channel;
    batch_channel.channel = QStringLiteral("B");
    batch_channel.name = QStringLiteral("通道 B");
    batch_channel.x = 40;
    batch_channel.y = 200;
    CHECK_TRUE("batch-add-top-node", batch_top.add_node(batch_channel) > 0);

    RuleGraph batch_plugin = RuleManager::instance().get_plugin_rule_graph("probe");
    const long long plugin_nodes_before = static_cast<long long>(batch_plugin.nodes().size());
    GraphNode batch_source;
    batch_source.type = GraphNodeType::Module;
    batch_source.value_id = QStringLiteral("hp");
    batch_source.name = QStringLiteral("生命值");
    batch_source.x = 20;
    batch_source.y = 240;
    CHECK_TRUE("batch-add-plugin-node", batch_plugin.add_node(batch_source) > 0);

    std::map<std::string, RuleGraph> batch_plugins;
    batch_plugins["probe"] = batch_plugin;
    std::vector<std::string> batch_failures;
    CHECK_TRUE("batch-save-all",
        RuleManager::instance().save_rule_graphs("rules.json", &batch_top, batch_plugins, batch_failures));
    CHECK_INT("batch-save-no-failure", 0, static_cast<int>(batch_failures.size()));

    // 两张文件都更新：总图多一个通道 B 节点，插件图多一个数值源节点
    RuleGraph reloaded_top;
    reloaded_top.load(QStringLiteral("rules.json"));
    CHECK_INT("batch-save-top-written", top_nodes_before + 1,
        static_cast<long long>(reloaded_top.nodes().size()));
    RuleGraph reloaded_plugin = RuleManager::instance().get_plugin_rule_graph("probe");
    CHECK_INT("batch-save-plugin-written", plugin_nodes_before + 1,
        static_cast<long long>(reloaded_plugin.nodes().size()));

    // 引用列表在批量保存末尾统一重建：插件图默认输出节点重新指向总规则图
    const int reloaded_output_id = reloaded_plugin.default_output_node();
    const GraphNode* reloaded_output = reloaded_plugin.find_node(reloaded_output_id);
    CHECK_TRUE("batch-save-references-synced", reloaded_output != nullptr
        && reloaded_output->references.contains(rule_graph_top_ref()));

    // 13b) 总图没有可写入的规则文件：记一条失败（空标识），插件图仍然写盘
    RuleGraph extra_plugin;
    const int extra_id = extra_plugin.add_node(batch_source);
    CHECK_TRUE("batch-partial-add-node", extra_id > 0);
    std::map<std::string, RuleGraph> partial_plugins;
    partial_plugins["probe2"] = extra_plugin;
    std::vector<std::string> partial_failures;
    CHECK_TRUE("batch-partial-not-all-ok",
        !RuleManager::instance().save_rule_graphs("", &batch_top, partial_plugins, partial_failures));
    CHECK_INT("batch-partial-failure-count", 1, static_cast<int>(partial_failures.size()));
    CHECK_STRING("batch-partial-failure-top", "",
        partial_failures.empty() ? std::string("missing") : partial_failures.front());
    CHECK_TRUE("batch-partial-other-written",
        RuleManager::instance().has_plugin_rule_graph("probe2"));

    if (failures == 0) {
        std::printf("rule_graph_eval_test: 全部用例通过\n");
        return 0;
    }
    std::fprintf(stderr, "rule_graph_eval_test: %d 处断言失败\n", failures);
    return 1;
}
