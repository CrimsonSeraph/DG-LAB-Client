# DG-LAB-Client 交接文档（阶段三进行中）

## 一、给新上下文的提示词（可直接粘贴）

```
你是 DG-LAB-Client 项目的资深 C++20 / Qt6 / QML 开发与任务编排代理，工作目录 F:\\Projects\\DG-LAB-Client。
先完整阅读根目录 HANDOFF.md，再读 CodingStyle.md、docs/theme.md、docs/event-mapping.md、docs/rule-graph.md。
严格遵守 HANDOFF.md 里的「不可变要求」与「环境事实」，不要重新发明规范。
当前处于阶段三（UI 重构）中后段：文档同步与「模块页并入插件中心」两批刚派发，需先核对它们的结果。
任何不确定的实现细节，先读代码取证再动手；任何产品口径不确定，先提问再实现。
```

## 二、项目与角色

- 项目：DG-LAB-Client，C++20 + Qt6/QML 桌面应用，分层 core / module / rule / wave / ble / net / ui / event。
- 你的角色：任务编排代理（dispatcher）——负责计划、派发、评审、验收与交付摘要；实现交给子代理，不亲自改代码（除极小且子代理无法完成的事）。
- 回复语言：中文。
- 用户会实机运行并随时打断，按要求推进；**每完成一个阶段或子模块要输出「改动内容 / 设计决策 / 下一步」并暂停等确认**。

## 三、不可变要求（用户明确要求，必须保留）

### 3.1 编码规范
- 严格遵循根目录 `CodingStyle.md`：4 空格缩进、K&R、snake_case、PascalCase 类名、Doxygen 注释、头文件分区顺序 public → protected → private → signals → private slots、源文件定义顺序与头文件声明顺序一致、`#pragma once`。
- 文件编码 UTF-8 无 BOM、CRLF、文件末尾一个空行。
- 每个函数都要有说明注释；简单函数一行 `//`，复杂函数用 Doxygen。注释**只说明功能，不解释原因**。
- 日志用 `LOG_MODULE` 宏；热点路径不刷日志，失败路径用限流/单次记录。

### 3.2 文档
- 每个目录的 README.md 与 docs/ 下文档必须与实现同步；改接口/字段/行为要同步更新对应文档。
- 文档简洁凝练、直接说明功能与用法，**严禁使用 `===` / `---` 分割线**。
- 修改大文档时必须单子串编辑，**禁止整文件读入再写回**（历史上因此丢失过 71 行）。

### 3.3 提交
- 每个小任务完成后 Git 提交，格式：
  ```
  <type>(<scope>): <subject>
  - <body>
  - <body>
  ```
  type 与 scope 用英文，subject 与 body 用中文，body 简述关键变动。
- 提交前要求构建通过（`cmake --build` 0 error）；能跑测试就跑到 ctest 全绿。
- 提交由编排代理统一执行，子代理禁止运行 git。

### 3.4 UI 视觉
- 参考截图在 `F:\\Projects\\img`（9 张）。与用户文字规划冲突时**以文字规划为准**。
- 颜色只用 Theme 令牌，禁止任何颜色字面量（hex / Qt.rgba / 命名色）。当前默认主题是 `classic`（近黑底 + 淡黄强调）。

## 四、环境事实（不要重新踩）

- Qt 6.9.3 msvc2022_64，Visual Studio 18 2026 生成器，MSVC。
- 构建目录是 `build-agent`（编排代理建的临时构建树，未入库）。构建命令必须带 TMP 覆盖，否则 MSBuild 因临时目录受限失败：
  ```bash
  cd /f/Projects/DG-LAB-Client && export TMP="$PWD/build-agent/tmp" TEMP="$PWD/build-agent/tmp" TMPDIR="$PWD/build-agent/tmp" && cmake --build build-agent --config Release --parallel 8
  ```
  前台调用有 300s 上限，长构建用 `run_in_background: true` + `job_output(wait: true)`。
- 测试：`cd build-agent && ctest -C Release --output-on-failure`（当前 2 个目标：event_engine_test、rule_graph_eval_test）。
- **构建前必须清实例**：`powershell -NoProfile -Command "Get-Process -Name 'DG-LAB-Client' -ErrorAction SilentlyContinue | Stop-Process -Force"`。共享 build-agent 时两个代理同时构建会 LNK1104。
- 本机 125% 缩放：截图脚本**必须 DPI 感知**（`SetProcessDpiAwarenessContext(-4)`），否则 GetWindowRect 给 ×0.8 假尺寸，会把正确布局误报成裁切。
- QML 运行期错误的唯一可靠抓法：
  ```bash
  cd build-agent/Release && (QT_FORCE_STDERR_LOGGING=1 QT_ASSUME_STDERR_HAS_CONSOLE=1 ./DG-LAB-Client.exe > out.txt 2> err.txt &) ; sleep 8
  ```
  验收要求 `err.txt` 0 字节。日志文件开关：config 里 `app.log.file_enabled=true` 会写 `<exe>/log/dglab-YYYYMMDD.log`。

## 五、已落地的关键架构决策

- 主题：17 套预设，默认 `classic`（DefaultConfigs + 出厂 config/user.json + ThemeManager 回退值四路统一）。
- 插件展示元数据：侧车 `<exe>/module/<dll主干名>/plugin.json`（源码侧 `module/<name>/meta/`，经 `dgl_add_plugin(META_DIR ...)` 部署）；展示名与模块标识分离，`IPlugin::name()` 仍是模块标识（规则引用依赖它，不得改）。
- 事件：声明在 sidecar 的 `events`；触发由宿主按「已注册数值 + compare」判定；映射存 `user.json`；事件**只产出数值**，走向与强度由规则决定；事件不落 enabled（只由级联决定）；新增 `dataType` ∈ strength/percent/wave；无子选项事件用隐式子选项 id `default`。
- 通道上限与安全上限**是通道级全局配置**：`app.channels.<A|B>.{limit,safety}`，首次读取从旧 per-plugin 值迁移一次；界面显示上限 = min(limit, safety)。
- 规则：插件级规则图 `config/rules/plugin_<插件>.json`，总图 `config/rules/rules.json` 的 `graph` 字段；节点类型 `pluginOutput`（仅总图）与 `defaultOutput`（每插件图唯一、不可增删）；求值先插件图后总图；启用级联按 `channel:A/B` 与 `graph:top|plugin:<id>` 生效。
- 设备分发：应用只做收集/处理/分发，向所有已连接设备同时下发，插件配置不选设备。
- 连接状态：蓝牙 0/1 + App WebSocket 在线客户端数（`DglabRelayServer::client_count()` → `DeviceController::appClientCount`），任一连接即「已连接设备 ×N」。
- 图标：Font Awesome Free 7.3.1 两个 otf 入库 + `AppIcon` 组件；**FA7 两字重共用族名，必须显式 font.weight（solid=Black）**，否则 solid-only 图标渲染成方框。

## 六、提交历史要点（便于回溯）

`git log --oneline` 关键节点：主题 17 套与 classic 默认、插件侧车元数据、事件引擎与测试、EventRuntime/EventBridge、规则数据层（插件图/输出节点）、规则求值+启用级联、全局通道上限+dataType、事件面板只读、图标与双连接、首页看板与波形卡、更新模块修复（含恢复被误删的 `is_newer_version`）。

## 七、当前状态与未完成事项

### 7.1 刚派发、需先核对结果（新上下文第一件事）
1. **文档同步**：`src/ui/README.md` objectName 表按代码真实值重写、信号处理器计数如实、`include/README.md` 节点表补两种新类型、`docs/rule-graph.md` 补迁移章节。要求脚本给出「文档有代码无 / 代码有文档无」双向差集。
2. **模块页并入插件中心**：查询周期控件搬进 `PluginCenterPage.qml`（**objectName 沿用 `modulePeriodCombo` / `modulePeriodApplyButton`**，C++ 零改动）、删除 `ModulePage.qml` 与 `QML_FILES` 条目、`MainWindow` 页栈删 `ModulePage {}`、`AppBridge::Page` 的 `ModulePage=2` **保留不重排**。
   核对方式：构建 + 启动，`err.txt` 必须 0 字节（删页面最易漏页栈引用）。

### 7.2 待办（按建议顺序）
- 设置页重组：日志独立成左侧「日志」页、蓝牙直连并入「连接」（WebSocket 扫码下方）、移除波形选项卡（波形改到首页，已部分完成）。
- 日志页扩展：暴露配置文件里已有的日志选项（导出目录、单文件大小上限、导出级别、仅导出某级别等），按实用性挑选。
- 关于页：移除软件更新卡片（左下角已有检查更新入口）、软件信息改为用户能懂的中文介绍、修 `release_notes` 4 行截断与 150px 硬编码间距。
- 迷你窗 + 托盘常驻：无边框、置顶、可拖动、锁定按钮、A/B 大数字与百分比、透明度与大小滑块、近 N 条触发流水、波形滚动播放；× 隐藏到托盘、退出走托盘菜单；接管强度调节与通道启停。
- 功能尾巴：`EventEngine::remove_module()`、事件运行时每帧结果到设备下发的接线（B0 的 100ms 写入已完成，缺的是这一步；且需先定事件输出与规则命令在同一通道上的合并 / 优先级规则，待产品口径确认）、插件卡点击定位到具体插件（需参数化导航）、「当前值」实时刷新（`ModuleManager::value_changed` → `events`）。
- 小尾巴：`updateApplyButton` 的 primary+danger 组合待确认；`watch_items` 全量重扫评估后不做（重扫只在容器 `childrenChanged` 时触发、不是每帧，且 `connected_items_` 已按控件去重；改成增量要额外维护委托反查索引，收益不抵复杂度）。

### 7.3 打磨与已知局限
- 主题化边界：`ColorDialog` 是 Windows 原生取色框，由系统绘制，无法套用 `Theme` 令牌。
- 换行：仓库部分文件 LF/CRLF 不一致（同一目录内也有混用），统一要单独排一次批量格式化。
- 退出耗时约 5.6s，全部落在 QML 引擎析构阶段，与应用逻辑无关。
- Windows 11 托盘图标默认收在折叠区，需用户手动拖到任务栏可见位置。

## 八、已知坑（血泪清单）

1. 位置器/布局（Column/Row/Grid/ColumnLayout…）的 `implicitWidth/implicitHeight` 是**只读**的；只有 Item/Rectangle 可写。写错会让整个页面类型不可用、主界面加载失败，而 qmllint 不报。
2. 用位置器作 `Flickable.contentHeight` 的来源时才可靠；读布局的 implicit 尺寸会得到 0 或时序不确定值。
3. 布局内子项用 `Layout.*` 附加属性，位置器内子项必须用 `width: parent.width` —— 换容器时要逐个核对。
4. 导航是 C++ 驱动的：`AppBridge::Page` + `UiConnector::nav_bindings()` 的 objectName 映射；QML 导航按钮不写 onClicked。页内按钮可以写。
5. 侧车 `banner` 只是文件名（QML 不要自己拼路径）：头图统一走 `image://modulebanner/<插件文件名>/<banner>`，由 `ModuleBannerImageProvider` 按 `<插件扫描目录>/<dll 主干名>/<banner>` 解析；带路径分隔符或文件不可读时返回空图，卡片与详情页保持空态。
6. 启用 CS2 插件会往本机 Steam 的 CS2 cfg 目录写 `gamestate_integration_dglab.cfg`（插件既有副作用，需提示用户）。
7. 用户自己的提交曾整段删掉 `is_newer_version` 函数体导致 LNK2019；接手大改动后先构建一次确认 HEAD 可编译。
8. 两个子代理同时构建同一 `build-agent` 会互相抢 exe（LNK1104）。同一产物只允许一个代理构建。
9. 截图脚本坐标要用**屏幕绝对坐标**（`SetCursorPos` 不收窗口相对偏移），且必须 DPI 感知。

## 九、验收模板（每次交付按此核对）

1. 构建 `cmake --build build-agent --config Release` 0 error；`ctest -C Release` 全绿。
2. 启动 `QT_FORCE_STDERR_LOGGING=1 QT_ASSUME_STDERR_HAS_CONSOLE=1`，`err.txt` 0 字节。
3. DPI 感知截图与需求/参考截图对照；界面无颜色字面量。
4. 文档同步项已更新（接口、字段、objectName、节点表）。
5. 数据落盘位置正确（`user.json` 的 `app.channels.*`、`app.plugins.<模块>.events.*`、`config/rules/*.json`）。
6. 按规范提交，并在回复里给出提交哈希与一句话摘要。
