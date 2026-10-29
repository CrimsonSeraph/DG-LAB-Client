# 规则图数据层

规则层分两级：**总规则图**（全局）与**插件规则图**（每个插件一张）。本文件描述数据模型、落盘格式、引用关系与依赖集合查询；编辑器界面（拖拽、引用文本跳转）在后续批次接入。

## 1. 文件布局

| 文件 | 内容 | 由图 API 读写 |
| --- | --- | --- |
| `config/rules/rules.json` | 总规则图：`graph` 字段；`rules` 字段为兼容旧格式的规则集合 | `RuleGraph::load/save`、`RuleManager::read_rule_graph_json/write_rule_graph_json` |
| `config/rules/plugin_<插件标识>.json` | 插件规则图：`graph`（图）+ `rules`（由规则节点派生的规则集合）；插件标识取插件的模块标识（`IPlugin::name()`），文件名随之而定（本机 CS2 插件的规则图是 `plugin_CS2 GSI 模块.json`） | `RuleManager::get_plugin_rule_graph/save_plugin_rule_graph` |
| `config/rules/<规则文件>.graph.json` | 旧版规则图侧车（只读兼容，保存时不再写回） | `RuleGraph::load` 的兜底分支 |

图的 JSON 形状：

```json
{
    "version": "1.0",
    "plugin": "my_plugin",          // 总规则图写空字符串
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
| `Channel` | `channel` | 通道输出 `A` / `B`（**数据汇**：上游接入后按目标设备下发，单来源） | 1 |
| `PluginOutput` | `pluginOutput` | **插件输出节点**：只出现在总规则图，代表某插件规则图的最终输出 | 0 |
| `DefaultOutput` | `defaultOutput` | **默认输出节点**：每个插件规则图恰好一个，本图唯一出口 | 1 |

两个新增节点都携带插件标识（`pluginId`）与引用列表（`references`）：

```json
{ "id": 5, "type": "defaultOutput", "name": "默认输出", "pluginId": "my_plugin", "references": [ "top" ] }
```

数据层不变量：

- 插件规则图恰好一个默认输出节点：载入时多余的节点被丢弃（保留首个），`RuleGraph::add_node` 不会新增第二个，`RuleGraph::remove_node` 拒绝删除默认输出节点。
- 插件输出节点没有输入端口，`RuleGraph::connect_nodes` 拒绝连入。
- 默认输出节点单来源，新连线会替换该端口上的既有连线。

### 2.1 通道节点（输出端点）

通道节点是图的**数据汇**，不是数据源：它只有一个输入端口，上游任意节点（数值源、运算、高级、规则、插件输出、默认输出等）把输出端口连到它上面，数据流入后由它负责下发。

- **按设备区分**：通道节点带目标设备字段 `targetDevice`。

| 取值 | 含义 |
| --- | --- |
| 空字符串（缺省） | 跟随默认设备：求值时按当时评选出的默认设备解析 |
| 设备 MAC（归一为大写） | 指定该设备；可以是已连接设备，也可以是已记忆但离线（`app.devices` 里出现过）的设备 |

- **同一设备同一通道只允许一条入边**：作用域键 = `<通道>@<设备>`（见 `rule_graph_channel_scope_key`）。往某设备某通道的通道节点连入新连线时，`RuleGraph::connect_nodes` 会断开同作用域下其它通道节点上的入边并记一条 INFO 日志；不同设备的同名通道、同一设备的不同通道互不影响。跟随默认设备在连入时按**当时的**默认设备落成具体设备，因此「跟随默认设备」的节点与「指定该设备」的节点落在同一作用域，不会出现两条入边同时生效。默认设备之后改变时不会回改已有连线（唯一性只在连入时判定）。
- **属性面板按节点类型构建**：编辑器右侧检查器用 `Loader` 按节点类型只构建适用控件——规则节点显示规则名 / 值模式 / 启用，自定义表达式节点显示表达式，通道节点只显示「目标设备」下拉与说明；不适用的控件根本不创建，而不是置灰。设备下拉的模型来自 `RuleGraphBridge::channelDeviceOptions`（第 0 项「跟随默认设备」，其余为在线输出设备 + 已记忆设备，离线项名称带「离线」），与设备管理页的上下限下拉同一份实现（`DeviceController::outputDeviceTargets()`）。
- **求值按目标设备分发**：求值器仍按输入端口取上游值，命令里额外带 `device` 字段（`{"cmd":"send_strength","channel":"A","mode":2,"value":47,"device":"D3:3B:5A:FA:43:0C"}`，空串表示跟随默认设备）。设备解析集中在 `DeviceController::send_device_strength()`：目标就是默认设备时沿用「规则 + 事件取较大值 → 通道上限」的合并下发；目标是另一台在线设备时按该设备的通道上下限直接下发，不参与默认设备的合并状态。
- **离线处理**：目标设备不在在线输出设备列表（含跟随默认设备但没有默认设备）时**不下发、不更新通道状态**，只记一条按「通道 + 目标」限流的 WARN（日志面板可查）。不弹 Toast：该判定会在每次求值帧重复出现，Toast 会持续刷屏。

## 3. 引用关系

图标识：总规则图 = `top`；插件规则图 = `plugin:<插件标识>`。显示文本由 `rule_graph_ref_display` 给出（`总规则图` / `<插件标识> 规则图`）。

- 总规则图里的插件输出节点写 `references: ["plugin:my_plugin"]`，表示它引用该插件图。
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

起点为通道节点或输出节点时得到「该输出依赖的子规则/事件」；`from_node_id = 0` 表示整图。总规则图查询会并入其插件输出节点对应插件图的依赖（只展开一层）：因此「启动总规则图的通道 A」即可拿到 `my_plugin` 图里的规则与事件。事件来自插件侧车声明（`ModuleManager::get_plugins()`），仅供启用级联判定，不改变求值流程。

## 5. C++ / 桥接 API

`RuleGraph`：`plugin_id()/set_plugin_id`、`default_output_node`、`ensure_default_output`、`plugin_output_nodes`、`output_references/set_output_references/add_output_reference`、`dependencies`、`import_from_plugin`。

`RuleManager`：`get_plugin_graph_file`、`get_plugin_rule_graphs`、`has_plugin_rule_graph`、`get_plugin_rule_graph`、`save_plugin_rule_graph`、`read_rule_graph_json`、`write_rule_graph_json`、`sync_output_references`、`get_output_referrers`、`get_graph_dependencies`、`fill_dependency_events`、`evaluate_graph_outputs`、`get_plugin_graph_output`、`get_graph_channel_output`、`set_graph_output_enabled`、`is_graph_output_enabled`、`get_cascade_enabled_rules`、`get_cascade_enabled_events`。

`RuleGraph`：`evaluate(node_id, resolve, plugin_outputs)` 求值某节点上游子图（`RuleGraph::ValueResolver` 解析模块数值）。

`EventRuntime`：`set_cascade_events` / `cascade_events`（运行期启用集合，供启用级联使用）。

`RuleGraphBridge`（QML 侧）：属性 `pluginGraphs`、`currentGraphPlugin`、`outputReferences`、`channelDeviceOptions`（通道节点目标设备下拉的模型）；方法 `loadPluginGraph`、`graphDependencies()`（整图）/ `graphDependencies(nodeId)`（某节点上游）、`outputReferrers`、`channelDeviceIndex(targetDevice)`、`setChannelDeviceTargetByIndex(nodeId, index)`；节点数据含 `pluginId`、`references`、`referenceLabels`、`targetDevice`、`deviceLabel`、`rowCount`（节点高度公式的行数来源）。`attach_device(DeviceController*)` 由 `UiConnector` 在装配时注入，用于取默认设备与设备目标列表。

通道节点相关的纯函数（`RuleGraph.h`，求值分发与唯一入边共用一份判定）：`rule_graph_normalize_device`、`rule_graph_channel_scope_key(channel, targetDevice, defaultDevice)`、`rule_graph_resolve_channel_target(targetDevice, defaultDevice, onlineDevices)`（返回 `ChannelTargetResolution`：`Ready` / `NoDefault` / `Offline`）；`RuleGraph::set_default_device` 把当前默认设备写进图，供唯一入边判定使用。

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
3. 通道节点在通道已启用时才生成 `send_strength`（`mode = 2`，即绝对强度，另带 `device` = 该节点的 `targetDevice`）经 `rule_command_ready` 下发；通道未启用时只记录求值结果。设备解析与离线判定在 `DeviceController::send_device_strength()` 里完成（见 2.1）。

节点求值口径与总图的规则节点一致：

| 节点 | 求值 |
| --- | --- |
| `module` | 由数值 ID 查模块当前值；无数据源或尚未取到值时为空（不产出） |
| `rule` | 按端口取值后走 `Rule::compute_value`（空值语义 + 模式钳位） |
| `operator` / `advanced` | 生成 `Math.floor(...)` 表达式，复用 `valuePattern` 的 QJSEngine 求值路径 |
| `channel` / `defaultOutput` | 取端口 0 的上游值（通道节点的目标设备不参与求值，只决定下发到哪台设备） |
| `pluginOutput` | 总图取对应插件图结果，插件图不存在该值按空处理；插件图内出现插件输出节点属当前数据层不支持的情形，按 0 处理并记一条限流 WARN |

插件图每次求值都从 `plugin_<插件标识>.json` 读取，因此保存后下一次求值即生效，不做额外的热重载订阅。

## 7. 启用级联

启动通道或图输出时，依赖集合（第 4 节）里的子规则与事件一并视为启用；停止时解除。级联状态只存在于运行期，不写入任何存储字段。

- 来源键：通道 `channel:A` / `channel:B`，图输出 `graph:top` / `graph:plugin:<插件标识>`；各来源的依赖集合取并集得到 `cascade_rules_` 与 `cascade_events_`，查询入口 `get_cascade_enabled_rules()` / `get_cascade_enabled_events()`。
- 规则侧：`is_rule_effectively_enabled` 对集合内规则直接判定为启用（插件图里的规则节点名未载入规则集合同样如此）。
- 事件侧：`EventRuntime` 维护 `std::set<std::string>` 白名单，`read_event_configs` 之后由 `apply_cascade_enabled` 覆盖 `enabled`，随后重新 `configure` 引擎，下一帧生效；`EventBridge` 展示同样按运行期集合显示为启用。
- 依赖集合为空时该来源不改变任何判定，行为与未接入级联前一致。

## 8. 编辑器草稿与批量保存

编辑器（`RuleGraphBridge` + `dialogs/RuleEditorDialog.qml`）在打开期间持有内存草稿，切图不重读磁盘、也不写盘：

| 状态 | 位置 | 含义 |
| --- | --- | --- |
| 当前图 | `RuleGraphBridge::graph_` | 正在编辑的那张图（唯一可改的实例） |
| 草稿 | `drafts_`（图标识 → 图） | 非当前图的草稿；切出时写入、切入时取出，因此草稿表里永远不含当前图 |
| 脏标记 | `dirty_graphs_`（图标识集合） | 每张图各自脏；`dirty`（聚合）与 `dirtyGraphs`（显示名列表）供界面使用 |

- 切图（`loadPluginGraph(plugin_id)`，空字符串 = 总规则图）：把当前图存进草稿 → 目标图命中草稿就直接取出 → 未命中（首次进入）才从磁盘载入；全过程不写盘。返回同一张图时直接返回，不重载。
- 保存（`save()`）：一次写全部脏图。顺序固定为**总规则图在前、插件图按标识排序**——插件图写盘时会重建默认输出节点的引用列表，需要读到最新的总图。写成功的图清脏；失败的图保留脏标记与草稿，界面汇总提示「哪几张失败、其余几张已保存」，不中断其余写入。
- 丢弃（`discardChanges()`）：关闭编辑器时选「放弃改动」→ 清空草稿表与脏标记，当前图按磁盘内容重新载入。换规则文件（`loadRuleFile`）同样作废全部草稿。
- 保存成功后，未脏的草稿会按磁盘内容重新载入一次，让默认输出节点的「被谁引用」立即反映新拓扑。

批量写盘由 `RuleManager::save_rule_graphs(top_file, top_graph, plugin_graphs, failures)` 承担：总图经 `write_rule_graph_json` + `modify_rule_file` 写入规则文件的 `graph` 与 `rules` 字段，插件图经 `write_plugin_rule_graph` 写入 `plugin_<插件标识>.json`，全部写完只调用一次 `sync_output_references()`；`failures` 按写入顺序给出失败条目（空字符串表示总图）。

插件中心里每个已启用插件的卡片带一个规则入口：没有 `config/rules/plugin_<插件标识>.json` 时显示「创建规则」，有则「编辑规则」。点击后先 `loadPluginGraph(插件标识)` 再打开编辑器，因此编辑器一打开就已经停在该插件图上（不是先开总图再切）；没有图文件时载入的是 `import_from_plugin()` 生成的默认图，首次保存才落盘。文案随启用状态与图文件存在性刷新，触发点是加载插件列表、启用 / 停用插件与保存规则图。

## 9. 迁移策略

1. 旧 `rules.json`（只有 `rules` 字段、无 `graph`）：`RuleGraph::load` 依次尝试规则文件的 `graph` 字段 → 旧版 `<规则文件>.graph.json` 侧车 → `import_from_rules()` 从规则集生成图。加载不丢数据，保存时把图写入规则文件的 `graph` 字段，侧车文件保留但不再更新。
2. 内置预置规则已从 `config/rules/rules.json` 移除（`rules` 为空对象），预置图只保留一个示例通道节点；插件图在首次编辑插件规则时按 `plugin_<插件标识>.json` 生成，其默认输出节点由总规则图的插件输出节点引用。老用户文件里的规则仍按第 1 条读入。
3. 旧侧车文件里的节点位置在首次保存后迁入规则文件；保存后节点 JSON 会补齐缺省字段（`op`、`advanced` 等），属正常归一。
4. `plugin_*.json` 与 `*.graph.json` 不再出现在规则文件列表（`get_available_rule_files`）中。
5. 通道节点的 `targetDevice` 是新增字段：旧规则文件（`config/rules/*.json`、`config/rules/rules.json`、插件图）里没有该字段时读作空字符串 = 跟随默认设备，读取过程不改写文件；只有用户保存该图时才会写出该字段。
