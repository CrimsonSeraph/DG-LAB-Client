# DG-LAB-Client 交接文档（阶段三已收口）

## 一、给新上下文的提示词（可直接粘贴）

```
你是 DG-LAB-Client 项目的资深 C++20 / Qt6 / QML 开发与任务编排代理，工作目录 F:\\Projects\\DG-LAB-Client。
先完整阅读根目录 HANDOFF.md，再读 CodingStyle.md、docs/theme.md、docs/event-mapping.md、docs/rule-graph.md。
严格遵守 HANDOFF.md 里的「不可变要求」与「环境事实」，不要重新发明规范。
阶段三（UI 重构）与波形库、设备链路扩展均已提交（见 7.1）。剩余待办见 7.2：新设备驱动需要实机才能验收，打磨项随时可做。
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
- 测试：`cd build-agent && ctest -C Release --output-on-failure`（当前 9/9 全绿，9 个目标：event_engine / rule_graph_eval / event_engine_module / event_merge / wave_library / wave_import_export / wave_zip / wave_tag / event_wave）。
- **构建前必须清实例**：`powershell -NoProfile -Command "Get-Process -Name 'DG-LAB-Client' -ErrorAction SilentlyContinue | Stop-Process -Force"`。共享 build-agent 时两个代理同时构建会 LNK1104。
- 本机 125% 缩放：截图脚本**必须 DPI 感知**（`SetProcessDpiAwarenessContext(-4)`），否则 GetWindowRect 给 ×0.8 假尺寸，会把正确布局误报成裁切。
- QML 运行期错误的唯一可靠抓法：
  ```bash
  cd build-agent/Release && (QT_FORCE_STDERR_LOGGING=1 QT_ASSUME_STDERR_HAS_CONSOLE=1 ./DG-LAB-Client.exe > out.txt 2> err.txt &) ; sleep 8
  ```
  验收要求 `err.txt` 0 字节。日志文件开关：config 里 `app.log.file_enabled=true` 会写 `<exe>/log/dglab-YYYYMMDD.log`。
- 界面取证禁用 PostMessage 注入：本机 PostMessage 注入的鼠标与键盘消息对该 Qt 进程无效（返回成功但界面无反应），历史上曾据此得出「已点击验证」的假结论。界面取证一律改用 **UI Automation**（InvokePattern / ValuePattern），或直接函数级单测 + PrintWindow 静态截图，并在报告里区分「界面验证」与「函数级验证」。

## 五、已落地的关键架构决策

- 主题：17 套预设，默认 `classic`（DefaultConfigs + 出厂 config/user.json + ThemeManager 回退值四路统一）。
- 插件展示元数据：侧车 `<exe>/module/<dll主干名>/plugin.json`（源码侧 `module/<name>/meta/`，经 `dgl_add_plugin(META_DIR ...)` 部署）；展示名与模块标识分离，`IPlugin::name()` 仍是模块标识（规则引用依赖它，不得改）。
- 事件：声明在 sidecar 的 `events`；触发由宿主按「已注册数值 + compare」判定；映射存 `user.json`；事件**只产出数值**，走向与强度由规则决定；事件不落 enabled（只由级联决定）；新增 `dataType` ∈ strength/percent/wave；无子选项事件用隐式子选项 id `default`。
- 通道上限与安全上限**是通道级全局配置**：`app.channels.<A|B>.{limit,safety}`，首次读取从旧 per-plugin 值迁移一次；界面显示上限 = min(limit, safety)。
- 规则：插件级规则图 `config/rules/plugin_<插件>.json`，总图 `config/rules/rules.json` 的 `graph` 字段；节点类型 `pluginOutput`（仅总图）与 `defaultOutput`（每插件图唯一、不可增删）；求值先插件图后总图；启用级联按 `channel:A/B` 与 `graph:top|plugin:<id>` 生效。
- 设备分发：应用只做收集/处理/分发，向所有已连接设备同时下发，插件配置不选设备。
- 连接状态：蓝牙 0/1 + App WebSocket 在线客户端数（`DglabRelayServer::client_count()` → `DeviceController::appClientCount`），任一连接即「已连接设备 ×N」。
- 图标：Font Awesome Free 7.3.1 两个 otf 入库 + `AppIcon` 组件；**FA7 两字重共用族名，必须显式 font.weight（solid=Black）**，否则 solid-only 图标渲染成方框。
- 波形库：官方 16 个波形作为只读资源（`assets/waves/official_waves.json`），`Wave` 同时持有 raw 与两种协议的帧（`frames_v2` / `frames_v3`），`app.wave.protocol` 决定发送用哪套、缺帧拒发。
- 波形导入导出：JSON / `.pulse` / `.zip`（vendor miniz，读支持 store 与 deflate，含路径穿越与大小上限防护）。
- 波形标签：两层库（内置为代码常量、自定义存 `config/waves/tags.json`），官方波形的标签是用户侧叠加数据（`official_wave_tags` 段）。
- 设备：`IOutputDevice` 抽象 + 47L1 型号识别（未支持型号拒绝连接）+ 每通道输出目标 `app.output.<A|B>.target`；事件波形（`dataType=wave`）按设备实际写入帧数推进。

## 六、提交历史要点（便于回溯）

`git log --oneline` 关键节点：主题 17 套与 classic 默认、插件侧车元数据、事件引擎与测试、EventRuntime/EventBridge、规则数据层（插件图/输出节点）、规则求值+启用级联、全局通道上限+dataType、事件面板只读、图标与双连接、首页看板与波形卡、更新模块修复（含恢复被误删的 `is_newer_version`）。当前 HEAD 以 `git log -1` 为准（阶段三 UI 重构与波形库、事件波形下发、输出设备抽象均已提交；工作区仅 `build-agent/`、`out/` 未跟踪）。

## 七、当前状态与未完成事项

### 7.1 已完成（阶段三收口）
- 主题 17 套、默认 `classic`；插件侧车元数据（展示名 / 作者 / 标签 / `banner`）。
- 事件引擎与映射：`dataType`（strength / percent / wave）、通道级全局上限、事件面板只读强度。
- 规则数据层与插件规则图；规则编辑器可编辑插件级连线。
- 首页看板：设备状态、通道卡、已启用插件、近期触发、通道波形卡。
- 日志页与波形设置页独立成页（原配置页的波形卡与日志卡迁入）；配置页只剩连接与主题，蓝牙直连并入连接卡。
- 插件详情三页签（模式与输出 / 触发事件 / 数值），数值页签由 C++ 侧 `selectModule` 驱动。
- 关于页重写；软件更新收进独立对话框，由左下角「检查更新」打开。
- 迷你窗：无边框置顶、标题条拖动与锁定、A/B 大数字与占比、强度增减、通道启停、透明度滑杆、大小三档、近 5 条触发流水、A/B 滚动波形；位置与尺寸等状态持久化。
- 系统托盘常驻；主窗口关闭改为隐藏到托盘，退出唯一路径是托盘菜单。
- 对话框统一继承 `AppDialog`；`AppMenu` 承接菜单外观；侧车头图经 `ModuleBannerImageProvider` 渲染。
- `EventEngine::remove_module` 与 `module_unregistered` 接线；`AppBridge::openPluginDetail` 直达插件详情。
- 事件运行时每帧结果 → 设备下发（strength / percent / wave 全部接线）：强度与规则命令按通道取较大值、按 `min(limit, safety)` 钳制、相同值不重复下发（`include/event/EventMath.h` 的 `ChannelOutputState`，测试目标 `event_merge_test`）。
- 事件波形（`dataType=wave`）：按设备实际 B0 写入帧数推进、播完恢复接管前的规则波形、缺帧拒绝并 Toast（纯函数 `advance_event_wave()`，测试目标 `event_wave_test`）；实机 47L121000 已核对，详见 `docs/event-mapping.md` 的「事件波形下发」。
- 验收基线：构建 0 error、`ctest` 9/9、启动 9 秒 `err` / `out` 均 0 字节。

### 7.2 剩余待办
- 新设备驱动：爪印 V1.1 / 灵猫 V1 / 负鼠 V1（协议文档已读，**需要实机才能验收**；负鼠还要先确认 B2 写 24 字节与文档标称 20 字节上限的冲突）。
- 打磨项：`ColorDialog` 为系统原生取色框无法主题化、退出耗时约 5.6s 全在 QML 引擎析构、Win11 托盘图标默认在折叠区、工作区有 31 个文本文件为 LF（`.gitattributes` 要求 CRLF，提交不受影响，属工作区卫生）。

### 7.3 打磨与已知局限
- 未启用插件的详情：通道上限已改为读通道级全局配置（不再是 `0/0`），但该插件的事件与数值列表仍为空，属预期。
- 配置热更只覆盖设备特有键：`ConfigManager::load()` 对已加载文件直接返回，外部手改 `user.json` 的 `app.devices.<MAC>.*` 靠 `DeviceController::refresh_limits()` 每 2 秒比对 mtime 的快照生效；通道级 `app.channels.*` 仍以应用内写入为准，外部手改需重启。要根治得让 core 的配置支持重读，属独立改造。
- 默认设备目前只有郊狼一个候选：负鼠驱动未实现前，「切换默认设备」的停旧波形与重发强度分支没有真实执行过。

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
