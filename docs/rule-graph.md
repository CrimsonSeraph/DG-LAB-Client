# 规则图数据层

规则层分两级：**总规则图**（全局）与**插件规则图**（每个插件一张）。本文件描述数据模型、落盘格式、引用关系与依赖集合查询；编辑器界面（拖拽、引用文本跳转）在后续批次接入。

## 1. 文件布局

| 文件 | 内容 | 由图 API 读写 |
| --- | --- | --- |
| `config/rules/rules.json` | 总规则图：`graph` 字段；`rules` 字段为兼容旧格式的规则集合 | `RuleGraph::load/save`、`RuleManager::read_rule_graph_json/write_rule_graph_json` |
| `config/rules/plugin_<插件标识>.json` | 插件规则图：`graph`（图）+ `rules`（由规则节点派生的规则集合） | `RuleManager::get_plugin_rule_graph/save_plugin_rule_graph` |
| `config/rules/<规则文件>.graph.json` | 旧版规则图侧车（只读兼容，保存时不再写回） | `RuleGraph::load` 的兜底分支 |

图的 JSON 形状：

```json
{
    "version": "1.0",
    "plugin": "buckshot",          // 总规则图写空字符串
    "nextNodeId": 6,
    "nextEdgeId": 4,
    "nodes": [ ... ],
    "edges": [ { "id": 3, "from": 4, "to": 5, "port": 0 } ]
}
```

规则文件内嵌图时，该对象位于规则文件的 `graph` 字段；插件图文件额外带 `plugin`、`kind: "pluginRuleGraph"` 与派生 `rules`。

## 2. 节点类型

| 类型 | JSON `type` | 语义 | 输入端口 |
| --- | --- | --- | --- |
| `Rule` | `rule` | 规则节点，端口数 = 表达式里 `{}` 的个数 | N |
| `Module` | `module` | 模块数值源 | 0 |
| `Operator` | `operator` | `+ - * /` | 2 |
| `Advanced` | `advanced` | `abs` / `square` / `sqrt` / `expression` | 1 或 N |
| `Channel` | `channel` | 通道输出 `A` / `B`（单来源） | 1 |
| `PluginOutput` | `pluginOutput` | **插件输出节点**：只出现在总规则图，代表某插件规则图的最终输出 | 0 |
| `DefaultOutput` | `defaultOutput` | **默认输出节点**：每个插件规则图恰好一个，本图唯一出口 | 1 |

两个新增节点都携带插件标识（`pluginId`）与引用列表（`references`）：

```json
{ "id": 5, "type": "defaultOutput", "name": "默认输出", "pluginId": "buckshot", "references": [ "top" ] }
```

数据层不变量：

- 插件规则图恰好一个默认输出节点：载入时多余的节点被丢弃（保留首个），`RuleGraph::add_node` 不会新增第二个，`RuleGraph::remove_node` 拒绝删除默认输出节点。
- 插件输出节点没有输入端口，`RuleGraph::connect_nodes` 拒绝连入。
- 默认输出节点单来源，新连线会替换该端口上的既有连线。

## 3. 引用关系

图标识：总规则图 = `top`；插件规则图 = `plugin:<插件标识>`。显示文本由 `rule_graph_ref_display` 给出（`总规则图` / `<插件标识> 规则图`）。

- 总规则图里的插件输出节点写 `references: ["plugin:buckshot"]`，表示它引用该插件图。
- 插件规则图的默认输出节点写 `references: ["top"]`，表示它被总规则图引用；被多个图引用时按「总规则图在前、其余按插件标识排序」排列。

`references` 由 `RuleManager::sync_output_references` 在每次保存后重建：扫描总规则图与全部插件图里的插件输出节点，写回被引用插件的默认输出节点。读取用 `RuleManager::get_output_referrers(plugin_id)`（原始图标识）与 `RuleGraph::output_references()`（当前图的引用列表）。

## 4. 依赖集合（启用级联）

`RuleManager::get_graph_dependencies(graph, from_node_id)` 返回 `GraphDependency`：

| 字段 | 含义 |
| --- | --- |
| `rules` | 上游可达的规则节点名 |
| `value_ids` | 上游可达的模块数值 ID |
| `channels` | 上游可达的通道（`A`/`B`） |
| `plugins` | 图涉及的插件标识（输出节点归属与引用） |
| `events` | 依赖插件声明的事件，写作 `<插件标识>/<事件 ID>` |

起点为通道节点或输出节点时得到「该输出依赖的子规则/事件」；`from_node_id = 0` 表示整图。总规则图查询会并入其插件输出节点对应插件图的依赖（只展开一层）：因此「启动总规则图的通道 A」即可拿到 `buckshot` 图里的规则与事件。事件来自插件侧车声明（`ModuleManager::get_plugins()`），仅供启用级联判定，不改变求值流程。

## 5. C++ / 桥接 API

`RuleGraph`：`plugin_id()/set_plugin_id`、`default_output_node`、`ensure_default_output`、`plugin_output_nodes`、`output_references/set_output_references/add_output_reference`、`dependencies`、`import_from_plugin`。

`RuleManager`：`get_plugin_graph_file`、`get_plugin_rule_graphs`、`has_plugin_rule_graph`、`get_plugin_rule_graph`、`save_plugin_rule_graph`、`read_rule_graph_json`、`write_rule_graph_json`、`sync_output_references`、`get_output_referrers`、`get_graph_dependencies`、`fill_dependency_events`、`evaluate_graph_outputs`、`get_plugin_graph_output`、`get_graph_channel_output`、`set_graph_output_enabled`、`is_graph_output_enabled`、`get_cascade_enabled_rules`、`get_cascade_enabled_events`。

`RuleGraph`：`evaluate(node_id, resolve, plugin_outputs)` 求值某节点上游子图（`RuleGraph::ValueResolver` 解析模块数值）。

`EventRuntime`：`set_cascade_events` / `cascade_events`（运行期启用集合，供启用级联使用）。

`RuleGraphBridge`（QML 侧，本批只读）：属性 `pluginGraphs`、`currentGraphPlugin`、`outputReferences`；方法 `loadPluginGraph`、`graphDependencies()`（整图）/ `graphDependencies(nodeId)`（某节点上游）、`outputReferrers`；节点数据新增 `pluginId`、`references`、`referenceLabels`。

## 6. 多图求值

求值顺序固定为「先插件图、后总规则图」，由 `RuleManager::evaluate_graph_outputs()` 统一驱动，调用时机与规则计算同一周期：

| 时机 | 触发点 |
| --- | --- |
| 模块数值变化 | `RuleManager::on_module_value_changed`（与规则级联计算同一次回调） |
| 通道启用 | `RuleManager::set_channel_enabled(channel, true)` 解锁后立即求值一帧 |
| 图输出启用 | `RuleManager::set_graph_output_enabled(graph_ref, true)` |
| 手动 | `RuleManager::evaluate_graph_outputs()` |

每一步的口径：

1. 插件图：以默认输出节点的上游子图为起点求值（`RuleGraph::evaluate`），结果缓存为 `get_plugin_graph_output(plugin_id)`。
2. 总规则图：`pluginOutput` 节点取对应插件图的上一步结果作为输入；通道节点据此得到强度，结果缓存为 `get_graph_channel_output(channel)`。
3. 通道节点在通道已启用时才生成 `send_strength`（`mode = 2`，即绝对强度）经 `rule_command_ready` 下发；通道未启用时只记录求值结果。

节点求值口径与总图的规则节点一致：

| 节点 | 求值 |
| --- | --- |
| `module` | 由数值 ID 查模块当前值；无数据源或尚未取到值时为空（不产出） |
| `rule` | 按端口取值后走 `Rule::compute_value`（空值语义 + 模式钳位） |
| `operator` / `advanced` | 生成 `Math.floor(...)` 表达式，复用 `valuePattern` 的 QJSEngine 求值路径 |
| `channel` / `defaultOutput` | 取端口 0 的上游值 |
| `pluginOutput` | 总图取对应插件图结果，插件图不存在该值按空处理；插件图内出现插件输出节点属当前数据层不支持的情形，按 0 处理并记一条限流 WARN |

插件图每次求值都从 `plugin_<插件标识>.json` 读取，因此保存后下一次求值即生效，不做额外的热重载订阅。

## 7. 启用级联

启动通道或图输出时，依赖集合（第 4 节）里的子规则与事件一并视为启用；停止时解除。级联状态只存在于运行期，不写入任何存储字段。

- 来源键：通道 `channel:A` / `channel:B`，图输出 `graph:top` / `graph:plugin:<插件标识>`；各来源的依赖集合取并集得到 `cascade_rules_` 与 `cascade_events_`，查询入口 `get_cascade_enabled_rules()` / `get_cascade_enabled_events()`。
- 规则侧：`is_rule_effectively_enabled` 对集合内规则直接判定为启用（插件图里的规则节点名未载入规则集合同样如此）。
- 事件侧：`EventRuntime` 维护 `std::set<std::string>` 白名单，`read_event_configs` 之后由 `apply_cascade_enabled` 覆盖 `enabled`，随后重新 `configure` 引擎，下一帧生效；`EventBridge` 展示同样按运行期集合显示为启用。
- 依赖集合为空时该来源不改变任何判定，行为与未接入级联前一致。

## 8. 迁移策略

1. 旧 `rules.json`（只有 `rules` 字段、无 `graph`）：`RuleGraph::load` 依次尝试规则文件的 `graph` 字段 → 旧版 `<规则文件>.graph.json` 侧车 → `import_from_rules()` 从规则集生成图。加载不丢数据，保存时把图写入规则文件的 `graph` 字段，侧车文件保留但不再更新。
2. 内置预置规则已从 `config/rules/rules.json` 移除（`rules` 为空对象），改为默认插件图样例 `config/rules/plugin_buckshot.json`，其默认输出节点由总规则图的插件输出节点引用。老用户文件里的规则仍按第 1 条读入。
3. 旧侧车文件里的节点位置在首次保存后迁入规则文件；保存后节点 JSON 会补齐缺省字段（`op`、`advanced` 等），属正常归一。
4. `plugin_*.json` 与 `*.graph.json` 不再出现在规则文件列表（`get_available_rule_files`）中。
