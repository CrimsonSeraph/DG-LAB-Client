# UI 优化方案（P0 可用性 → P1 视觉体系 → P2 交互细节）

> 本文档是 DG-LAB-Client 界面层的优化路线图与施工规格。结论来自 2026-09-30 的实机截图与源码审计，不引用 `screenshot/` 下的旧版本截图（那批图是已移除的 Widgets 界面）。
>
> 证据标记：〔实机〕= 本次启动 `build/Release/DG-LAB-Client.exe` 抓到的真实画面（1280×800 逻辑尺寸，125% DPI，窗口 1618×1047 含边框）；〔源码〕= 直接阅读源码确认；〔审计〕= 只读审计给出 `文件:行号`，已抽查复核。
>
> 相关文档：`src/ui/README.md`（交互契约与 objectName 表）、`docs/theme.md`（颜色令牌与预设）。

---

## 一、已定决策

| 决策 | 结论 | 影响 |
| --- | --- | --- |
| 优化主线 | P0 可用性 → P1 视觉体系 → P2 交互细节，分阶段推进 | 本文档按此排序；每阶段内部仍按"一处修复覆盖多处"排序 |
| 交付形态 | 方案写入本文档，本次不改代码 | 施工按第七节的批次推进 |
| 交互约束 | **不放宽**「QML 不写信号处理器」：新增语义交互一律走 C++ `UiConnector` | 凡需要新交互的条目，都附带 C++ 改动清单（第六节） |
| 视觉验证 | 允许启动 `build/Release/DG-LAB-Client.exe` 抓真实截图；不使用旧截图 | 第八节给出可复现的截图回归流程 |

### 不做的事（YAGNI）

- 不换技术栈、不引入第三方 UI 库、不改 `ThemeManager` 的主题派生模型（16 套预设保留）。
- 不动业务逻辑：规则引擎、插件加载、设备通信协议一律不改行为，只补"状态回传"。
- 不做 i18n 框架改造、不做深浅色自动跟随系统（现有 16 套预设已覆盖）。
- 不为了"现代感"引入玻璃拟态、渐变、装饰性动效；`GlassCard` 保持普通容器语义，只考虑改名或补浮层层级。

---

## 二、现状基线

### 2.1 结构与约束〔源码〕

| 项 | 现状 |
| --- | --- |
| 平台 | Qt 6 / QML 桌面应用，`ApplicationWindow` 1280×800，最小 900×600（`style/Responsive.qml:6-7`） |
| 页面 | 首页（A/B 通道面板）、配置（连接/波形/主题/日志）、模块、关于；对话框 3 个（规则编辑器、波形编辑器、波形库） |
| 令牌 | `Metrics`（间距/圆角/尺寸）、`Typography`（9 档字号）、`Responsive`（2 个宽度断点）、`ComponentStyle`（组件度量）；颜色在 C++ `Theme` 单例（16 套预设 + 自定义） |
| 交互契约 | QML 零信号处理器，全部由 `UiConnector` 按 `objectName` 连接；唯一登记例外是规则编辑器画布（`src/ui/README.md` 例外表） |

### 2.2 实机画面暴露的问题〔实机〕

四页全量截图（本轮工作目录 `%TEMP%/dgui/`：`p1-home.png`、`pg-config.png`、`p3-module.png`、`p4-about.png`）：

| 页面 | 观察 |
| --- | --- |
| 首页 | A/B 两张卡片占满可视高度，内容只填到 55%，下方大片空白；每张卡片内又嵌 4 个带边框的区块（强度/模块/规则/波形），容器与内容用同一视觉语法；强度行出现两个「－/＋」与一个带上下指示器的 SpinBox，三套入口并列；「启动通道」整幅填充强调色，是全页唯一强焦点，却不是最高频操作；A 卡片的「上限 200」贴在 ＋ 右侧，B 卡片同一文案被推到最右——同一组件两处布局不一致 |
| 配置 | 连接卡片内 IP/端口挤在左上，250px 二维码独占左侧，右侧约 900px 全空；「通道 B」波形预览被压成约 76px 窄条，而「通道 A」占满整行；主题卡片的主/副色块（#e8f0fe / #d4e6f1）与卡片底色几乎同色，色块失去意义；卡片底部被裁，日志区需滚动才可见；右缘出现一条与内容无关的滚动条 |
| 模块 | 查询周期一行之后是两张 232×118 的插件卡，卡片下方约 700px 空白；插件卡「暂未加载 / 启用后将加载」用最小字号承载全部状态信息，右下「启用」按钮无边框差异 |
| 关于 | 一张卡片三行字：应用名、版本、一句描述；文件注释声明的许可证与依赖信息并不存在 |

根因不是配色，而是**布局不表达优先级**：每一页都是"全宽卡片 + 等权区块 + 大量留白"，主任务（调强度、看连接状态、扫码配网）没有视觉起点。

### 2.3 审计给出的量化基线〔审计〕

| 维度 | 结论 |
| --- | --- |
| 颜色令牌纪律 | 已达标：4 个页面文件共 1 处颜色字面量（`ConfigPage.qml:498` 的 `"transparent"`） |
| 数字令牌纪律 | 未达标：35 处尺寸字面量，34 处集中在 `ConfigPage.qml`（26）与 `ModulePage.qml`（8） |
| 组件状态覆盖 | `AppButton`/`NavButton`/`AppTextField`/`AppTextArea` 完全没有禁用态与焦点环；9 个组件全部无 loading 态；全仓无 Toast/BusyIndicator |
| 对比度 | 四个主文字令牌在构造上达标（surface 7:1、surfaceAlt 4.5:1）；**16/16 套主题**都有至少一处未覆盖的组合色（`accentText` 画在 `pressed`/`danger` 上、`accent` 画在 `selectionBg` 上、语义色画在 `surfaceAlt` 上），最低 3.03:1 |
| 字号 | `fontCaption`（10px）用了 35 处、`fontSmall`（11px）15 处，其中 22 处承载状态/数值（不是装饰）；`fontTiny`（9px）与 `fontDisplay`（24px）全仓零引用 |
| 状态回传 | QML 能读到的桥接属性里没有任何错误文案、没有真实"连接中"阶段、没有输出可用性、没有保存失败原因 |

---

## 三、五个根因

1. **布局不表达优先级**：页面 = 全宽卡片堆叠，容器与内容等权，留白集中在正文下方而不是用于分隔任务。
2. **盒套盒**：`GlassCard` 与内部区块都是「1px border + surface 色」，层级只能靠嵌套表达，越嵌越糊。
3. **状态不可见**：禁用（无视觉）、连接中（假状态）、错误（只落 28px 状态栏）、输出不可用（无属性）、脏数据（仅一个 *）、忙碌（无 loading 组件）。
4. **密度与可读性**：10px 承载状态数据；字号阶梯 9–24 共 9 档，其中 13/14/16 相差 1–2px 不可辨识。
5. **对话框是另一个产品**：写死尺寸、无滚动、无遮罩、无 footer、无脏数据拦截、错误无处显示。

---

## 四、P0 · 可用性（阻断级，全部可独立验收）

> 排序依据：先修"一次操作即废"与"看起来能点其实点不到"，再修视觉。每项验收方式见第八节。

### P0-1 规则编辑器连线后画布死锁〔源码〕

- 位置：`dialogs/RuleEditorDialog.qml:462` 写 `canvas.connectFrom = 0;`，而三处守卫是 `>= 0`（`:281`、`:437`、`:457`）。
- 现象：完成一次连线后 `connectFrom` 停在 0，守卫恒真 → 虚线临时线常驻；`onPositionChanged` 在 `:437` 提前 `return`，节点拖动与画布平移全部失效；再次在输入端口松手会调 `connectNodes(0, …)`。
- 影响：核心编辑流程一次即废，用户只会认为"编辑器坏了"。
- 改动：`:462` 改为 `-1`（与 `:179`/`:464` 的 -1 哨兵一致），或把三处守卫统一为 `> 0`；两处都要加注释说明 0 是非法节点 id。
- 验收：连续连 3 条线 → 临时线消失、节点仍可拖动、画布仍可平移。

### P0-2 禁用态无视觉（一处修复覆盖 27 处按钮）〔源码〕

- 位置：`components/AppButton.qml:16-38` 的 `background` 与 `contentItem` 都没有 `control.enabled` 分支；`NavButton.qml` 同。
- 命中调用点：`ConfigPage.qml:89-94`（连接中）、`:337-341`（扫描中）、`:343-348`（未连接不能断开）。
- 影响：用户看不出按钮已禁用，反复点击无响应，判定为"软件卡死"。
- 改动：组件内加 `opacity: control.enabled ? 1.0 : Metrics.disabledOpacity`、文字取新增的 `Theme.textDisabled`、非强调态底色取 `Theme.surfaceDisabled`。**先加令牌再改组件**（令牌清单见 5.1）。
- 验收：900×600 下打开配置页，未连接时「断开/扫描」明显灰化；点击无响应但不再让人以为卡死。

### P0-3 日志无法向上滚动〔源码〕

- 位置：`ConfigPage.qml:512-513` —— `ListView` 上写了无条件绑定 `contentY: Math.max(0, contentHeight - height)`，任何滚动立即被覆写。
- 影响：日志卡是排障入口，历史不可读等于没有日志面板。
- 改动：新增跟随状态（如 `property bool stickToBottom`，按 `contentY` 与阈值维护），绑定加 `when: stickToBottom`；用户上滚即解除跟随，滚回底部恢复。QML 内不写 `onXxx`。
- 验收：滚动上移后保持位置；新日志到达不把视图拽回底部；滚回底部后恢复跟随。

### P0-4 溢出不可达：通道卡片与模块页〔审计〕

- 位置：`home/ChannelPanel.qml:23-274`（`Column` 无 `clip` 无滚动）、`pages/ModulePage.qml:69-149`（`Flow` 无滚动）、`components/GlassCard.qml` 无 `clip`。
- 影响：模块/规则条目一多（约 10 条），「启动通道」按钮被挤出卡片与可视区，即回不到关闭入口；模块卡超出高度后不可见不可点。
- 改动：卡片内容区改 `Flickable`/`ScrollView` + `clip: true`；模块/规则区块限高（新增 `ComponentStyle.panelBlockMaxHeight`）；启停按钮固定在卡片底部（`anchors.bottom`），不随内容滚动。
- 验收：注入 12 条模块与 12 条规则，启停按钮仍在卡片内且可点；内容区可滚动。

### P0-5 对话框在最小窗口下溢出〔审计〕

- 位置与数值：`RuleEditorDialog.qml:18-19` 1100×700、`WaveEditorDialog.qml:13-14` 900×640、`WaveSelectDialog.qml:13-14` 540×500；对照 `Responsive.qml:6-7` 最小窗口 900×600。
- 影响：900×600 下规则编辑器超出 200×100，「保存规则图」/「关闭」不可达；三个对话框都没有整体滚动。
- 改动：`width: Math.min(令牌, Overlay.overlay.width - 2*Metrics.spacingXl)`、`height` 同理；面板宽与世界尺寸进 `ComponentStyle`；内容区加滚动兜底。
- 验收：900×600 下打开三个对话框，主操作按钮全部可见可点。

### P0-6 未保存改动无拦截〔审计〕

- 位置：`RuleEditorDialog.qml:615` 用 `dirty` 显示「保存规则图 *」，但 `:620-625` 的关闭按钮直接 `close()`；全仓无 `closePolicy` 声明，模态 Popup 默认 Esc 与点击遮罩即关闭。`WaveEditorDialog.qml:312-316` 连脏标记都没有。
- 影响：按 Esc 或点外部即静默丢弃整张规则图/波形草稿。
- 改动：三个对话框显式 `closePolicy: Popup.NoAutoClose`；关闭路径统一走「dirty ? 确认 : close」，确认文案由 C++ 提供（`ruleGraph.dirty` / `waveBridge` 新增 dirty）；波形编辑器补脏标记与「段落切换前自动应用」。
- 验收：改节点后按 Esc → 弹出确认；取消 → 改动保留。

### P0-7 设备/输出状态不可见〔审计〕

- 缺口（`include/ui` 现状）：`DeviceController.h:88` 只有 `errorOccurred(QString)` 信号、无错误文案属性，QML 零引用；`connecting` 在 `DeviceController.cpp:145-150` 内同步 true→false，是假状态；`output_available()`（`DeviceController.h:51`）不是属性；`HomeBridge.cpp:114-117` 在输出不可用时静默丢弃强度、`:88-95` 的启停只写规则变量。
- 影响：用户以为在控制设备，实际没有下发。这是本方案里唯一的"安全级"缺陷。
- 改动：新增 4 个桥接属性 + 卡片内状态行（见第六节批次 1）。
- 验收：断开设备后首页通道卡出现「未连接输出」警示行；点「启动通道」后状态显示「已启动（无输出）」而非纯高亮。

### P0-8 启停与强度入口〔审计〕

- 位置：`ChannelPanel.qml:267-273` 启停无确认、无 `enabled`；`ConfigPage.qml:405-437` 蓝牙 A±/B± 无 `enabled`（同卡「断开」有）；`ChannelPanel.qml:65-69` 的 `Binding` 无 `when` 守卫，键入中间值可能被立即覆盖。
- 改动：启停加二次确认（或未连接时禁用）；蓝牙强度按钮统一 `enabled: ble.connected`；`Binding` 加 `when: !strengthSpin.activeFocus`。
- 验收：未连接时按钮灰化；手输强度不被吞字；关闭通道需确认。

---

## 五、P1 · 视觉体系

### 5.1 令牌扩充（先加令牌，再改页面）

| 新增令牌 | 建议值 | 解决什么 | 落地位置 |
| --- | --- | --- | --- |
| `Theme.focusRing` | 取 `accent` | 焦点环目前 4 处各写死 `border.width: 2`，还有 3 种不一致表现（`AppTextField.qml:19-20`、`AppSpinBox.qml:68-69`、`AppComboBox.qml:84` 只换色） | `ThemeManager` |
| `Theme.textDisabled` / `surfaceDisabled` / `borderDisabled` | 由 `textMuted`/`surfaceSubtle` 派生 | 禁用态现在复用 `textMuted` 或魔法不透明度 0.5/0.65 | `ThemeManager` |
| `Theme.scrim` / `scrimOpacity` | `windowBg` @ 0.45 | 三个对话框无遮罩，层次只靠 1px 描边 | `ThemeManager` |
| `Theme.dangerOnAlt` / `warningOnAlt` / `successOnAlt` | 对 `surfaceAlt` 重新校正 | `ThemeManager.cpp:267-269` 只对 `surface` 校正，night 下 danger/warning/success 在 `surfaceAlt` 仅 3.56–3.58:1 | `ThemeManager` |
| `Metrics.focusRingWidth` / `focusRingOffset` | 2 / 1 | 收口焦点描边宽度 | `Metrics` |
| `Metrics.disabledOpacity` | 0.45 | 收口 0.5/0.65/0.4 三种魔法值 | `Metrics` |
| `Metrics.iconSm/Md/Lg` | 12/14/18 | 图标尺寸目前分散在 `ComponentStyle` 与字号里 | `Metrics` |
| `Metrics.elevation1/2` + `Theme.shadow` | 浮层专用 | 全仓无阴影，浮层与卡片同层 | `Metrics`/`ThemeManager` |
| `Typography.fontMonoFamily` | Consolas → Cascadia Mono → monospace | `font.family: "Consolas"` 硬编码 3 处（`RuleEditorDialog.qml:587`、`WaveEditorDialog.qml:270`、`ConfigPage.qml:506`） | `Typography` |
| `Typography.lineHeightBody/Caption` | 1.5 / 1.4 | 全仓无行高令牌，密集表格靠猜 | `Typography` |
| `Motion.durationFast/Base` + `easingStandard` | 120/200ms | 全仓 0 处 `Behavior`，状态切换是硬跳 | 新增 `style/Motion.qml` 单例（需在 CMake 登记 `QML_FILES`；单例受"逐文件 `QT_QML_SINGLETON_TYPE`"限制，见 `src/ui/README.md`） |

补充进 `ComponentStyle`（消除 35 处尺寸字面量）：`logRowHeight`(18)、`logListHeight`、`moduleCardWidth/Height`(232/118)、`dialogRuleEditorW/H`、`dialogWaveEditorW/H`、`dialogWaveSelectW/H`、`graphWorldWidth/Height`(4000/3000)、`graphGridSize`（与世界网格对齐）、`panelBlockMaxHeight`、`portHitTolerance`、`listRowHeight`。

### 5.2 字号阶梯收敛

现状 9 档（9/10/11/12/13/14/16/18/24），差异 1–2px 的档位不可辨识，且 10px 承载 35 处文本、其中 22 处是状态数据。

- 收敛为 5 档：**12 / 14 / 16 / 18 / 24**（正文 / 次级标题 / 卡片标题 / 页面标题 / 展示）。
- 删除 `fontTiny`(9) 与 `fontCaption`(10)；原 10px 的标签与状态文本升到 12px，靠字重与色阶（`textSecondary`/`textMuted`）分层，而不是靠字号。
- 12px = 9pt，等于 Windows 默认 UI 字号，是下限而非目标；中文界面正文不建议低于 12px。

### 5.3 层级语法（消除盒套盒）

定义三级表面，并在文档里写死允许用法：

| 级别 | 用法 | 视觉 |
| --- | --- | --- |
| 页面底色 | 页面背景 | `windowBg`，无描边 |
| 卡片 | 一个任务单元（连接、波形、主题、日志） | `surface` + 1px `border`，圆角 12 |
| 区块 | 卡片内的分组 | **无描边**，`surfaceAlt` 或仅留白 + 小标题；不再嵌套 border |
| 浮层 | 对话框、菜单、下拉 | 高一级表面 + `Theme.shadow`/`elevation2` + `scrim` 遮罩 |

现状 `ChannelPanel` 在卡片内再套 4 个带边框区块，正是违反此表的典型。

### 5.4a 不变量：A/B 通道对等（用户约束）

A 与 B 是平级主体，不是"主通道 + 次通道"。界面上必须满足：

- 两个通道区域**任何时候等宽**（窗口从最小宽度到宽屏都成立），差值按像素算 ≤2px。
- 内部布局**逐元素对齐**：同一行元素（强度输入、± 按钮、上限提示）在两卡中的相对位置一致，不允许一侧被挤到边缘、另一侧紧跟内容。
- 同一类内容（波形预览、模块列表、规则列表）在两卡中宽度一致、行高一致。
- 折叠/紧凑状态下也保持对等，不允许一侧因文本更长而变宽。

实现约束：不能用固定像素宽度凑，必须用布局权重表达（`Layout.fillWidth` + `Layout.preferredWidth` 归零，或对半分权重），否则窗口缩放时会再次失衡。文本长度差异只能通过 `elide` 消化，不得影响容器宽度。

已知违反点（本方案实测）：首页两卡因隐含宽度差异导致约 2:1 的宽度失衡；配置页「通道 B」波形预览被挤成约 76px 窄条而 A 占满整行。

### 5.4 首页重构（主任务优先）

- 通道卡改三段式：**通道头（名称 + 运行状态徽标 + 启停）→ 强度控制行（单一入口：SpinBox + ±）→ 波形区**；模块/规则明细折叠为「详情」，默认收起（渐进披露），收起时只显示一行摘要（如「2 模块 · 1 规则」）。
- 启停按钮不再整幅填充：运行中用 `danger` 轮廓 + 状态徽标，而不是把整行涂成实心块。
- 强度只留一套入口：去掉 `－`/`＋` 按钮或去掉 SpinBox 的内置指示器（`AppSpinBox.qml:27-61` 已有指示器），二者不可并存；补单位说明（现有「上限 200」无单位）。
- A/B 两卡的强度行布局必须一致（实机发现 A 卡片「上限」贴左、B 卡片推到最右，同一段 `ChannelPanel.qml:55-80` 渲染出两种结果，需定位 `RowLayout` 的 `fillWidth` 项宽度来源）。
- 首页头部增加一行**连接状态摘要**（设备/中转/蓝牙三态），把现在只在配置页可见的连接状态带到主操作页。

### 5.5 配置页重构

- 连接卡改两列：左列表单（地址/端口/连接/状态行/错误），右列二维码 + 配对链接（当前二维码独占左侧、右侧 900px 空置）。
- 波形卡：目标通道 A/B 用「单选 + 文字标签」表达（现在只靠 `primary` 色差，纯颜色表意）；A/B 预览等宽布局（实机 B 预览塌陷，需定位 `ConfigPage.qml:153-208` 的宽度约束）。
- 主题卡：色块必须有边框/底色差，并在色块上叠加「主色/副色」标签而不是把十六进制当标题（`ConfigPage.qml:265/285`）。
- 日志卡：行高进令牌、正文 10px → 12px、级别配色改由 C++ 提供语义（`ConfigPage.qml:505` 的 `level >= 3` 魔法数）、卡片高度自适应（现 `Layout.preferredHeight: 260` 与内层 `170` 不自洽）。

### 5.6 导航与状态反馈

- 导航四项补图标（或去掉折叠后的单字「首/配/模/关」）：折叠态只剩单字既不像图标也不像标签，`MainWindow.qml` 的 `compactNav` 分支需要真正的最小形态（图标 + tooltip）。
- 选中态改为「左侧 2px 强调条 + `selectionBg` 底 + `accent` 文字」，并与焦点态区分（现在选中态只换底色，焦点态不存在）。
- 新增 `Toast` 组件承接一次性反馈（保存成功/失败、导出路径、扫描结果），替代「只写状态栏 28px 一行、永不消失」的现状；状态栏保留总览并增加分级色（`statusLevel`）。
- 修正对比度：把 `accentText`/`accent`/`selectionBg`/`pressed`/`danger` 的组合纳入 `recompute()` 的校正范围，目标 ≥4.5:1；已知最低 3.03:1（`accentText` on `Qt.darker(danger,1.2)`，`AppButton.qml:20+33`）。

### 5.7 模块页与关于页

- 模块页：插件卡改「名称 + 状态徽标 + 启用开关」，禁用态不压暗整卡（现 `ModulePage.qml:86` 的 `opacity: 0.65` 连带压暗「启用」按钮）；补空态（「未发现模块，检查 plugins 目录」）；查询周期从「四分之一秒」这种口语值改为「250 ms」并给出数值入口。
- 关于页：补许可证（GPL-3.0-only）、依赖（Qt 6）、仓库地址；正文色阶不要比版本号更弱（现正文 `textMuted`、版本 `textSecondary`）。

---

## 六、C++ 接线改动清单（保持 QML 零信号处理器）

> 成本档：**A** = QML 加 `objectName` + `UiConnector` 一行连接（2 文件 3 处）；**A′** = 委托热区走 `watch_items`（2 文件 3 处）；**B** = 新增桥接属性/信号（3–4 文件 4–6 处，含头文件 `Q_PROPERTY` 与 `emit`）。均不需要改 CMake；仓库无自动化信号测试（无 `tests` 目标），验收靠第八节手工清单。

### 批次 1（P0 必须，一次做完避免反复改 `UiConnector` 构造）

| # | 类 | 新增 | 消费方 |
| --- | --- | --- | --- |
| 1 | `DeviceController` | `Q_PROPERTY(QString lastError)` + `Q_PROPERTY(bool outputReady)` + `Q_PROPERTY(int pairingStage)` | 配置页连接卡状态行、首页连接摘要 |
| 2 | `HomeBridge` | `Q_PROPERTY(QString channelAState/channelBState)`（idle/armed/runningNoOutput） | `ChannelPanel` 状态徽标与警示行 |
| 3 | `AppBridge` | `Q_PROPERTY(int statusLevel)`（info/success/warning/danger）+ 可选自动清除 | 状态栏分级色、Toast |
| 4 | `UiConnector` | 把 `DeviceController::errorOccurred`、`RuleGraphBridge::statusMessage` 转发到 `app.statusText`（照 `UiConnector.cpp:61-68` 现有 LogBridge 写法） | 全局反馈 |

### 批次 2（P1 反馈类）

| # | 类 | 新增 |
| --- | --- | --- |
| 5 | `WaveBridge` | `Q_PROPERTY(QString lastError)`、`signal draftSent(QString channel, int seconds)`、`Q_PROPERTY(bool draftDirty)` |
| 6 | `RuleGraphBridge` | `Q_PROPERTY(QString lastMessage)`（现有 5 处 `emit statusMessage` 已有内容） |
| 7 | `ThemeManager` | `Q_PROPERTY(bool applying)` + `signal themeApplied(QString name)` |
| 8 | `ModuleBridge` | `signal pluginToggled(QString file, bool ok, QString error)` |
| 9 | `LogBridge` | `Q_PROPERTY(QString lastExportPath)`（QML 现在完全不引用 `logBridge`） |

### 批次 3（P2）

| # | 内容 | 成本 |
| --- | --- | --- |
| 10 | 对话框「脏数据确认」需要 C++ 提供确认动作 | B/A |
| 11 | 画布「删除连线」「适配视图」「新增节点级联偏移」（落点现写死 300,120 等，`:98-148`） | B |
| 12 | 列表/热区键盘激活（裸 `MouseArea` 改为可聚焦热区，激活走 C++） | A′ |

### 现有契约缺口（与批次 1 一起修）

- `src/ui/README.md:140` 把 `UiConnector::watch_items()` 写成 `watch_list()`，需更正。
- 规则编辑器内有 **15 处 QML 信号处理器落在例外表之外**（`:78`、`:98-148` 的 11 个调色板按钮、`:597`、`:617`、`:624`）。已核实：`ruleAdd*`/`ruleSaveButton`/`ruleEditorCloseButton` 在 `UiConnector.cpp` 里**没有**对应连接（`grep ruleAdd` 仅命中 QML），因此不存在"一次点击创建两个节点"的重复执行，但那 11 个 `objectName` 未进 `README.md` 的契约表。处置二选一（推荐前者）：补进例外表并登记契约；或删掉 QML `onClicked` 改走 `UiConnector`。

---

## 七、施工批次

| 批次 | 内容 | 交付判据 | 回归范围 |
| --- | --- | --- | --- |
| 0 | 加令牌（5.1）、字号收敛、`AppButton`/`NavButton` 禁用态 + 焦点态 | 页面内无新增字面量（`grep` 可查） | 全站视觉 |
| 1 | P0-1、P0-2、P0-4、P0-5、P0-8（QML 部分）+ 字面量与字号收敛 | 已完成（`a549718`） | 规则编辑器、配置页、模块页、通道卡 |
| 2 | P0-7 状态可见性、P0-3 日志跟随、P0-6 脏数据确认、启动期错误 | P0-7 已完成（`6761ea9`），其余进行中 | 首页、配置页、状态栏、对话框 |
| 3 | 视觉体系：5.3 层级语法、5.4 首页、5.5 配置页、5.6 导航与 Toast | 三尺寸截图对比通过、对比度全部 ≥4.5:1 | 全部页面 |
| 4 | C++ 批次 2 + 5.7 模块/关于页 | 保存失败可见；模块空态可见 | 对话框、模块页 |
| 5 | P2 交互细节（焦点体系、画布交互、footer、微文案） | 键盘可完成主要流程 | 对话框 |

顺序理由：批次 0 的令牌是后续所有视觉改动的公共依赖；批次 1 全是「零新交互」的修复，可在不改 C++ 的前提下独立发布；批次 2 才动桥接层，因为它是唯一需要同时改 `UiConnector` 与多个桥接类的批次。

---

## 八、验证方案

### 8.1 截图回归（本轮已实测可行）

1. 启动：`build/Release/DG-LAB-Client.exe`（同目录已带齐 Qt DLL，无需部署）。
2. 抓图：`cap.ps1 -TitlePart "DG-LAB-Client [" -TargetPid <pid> -TimeoutSec 15 -OutPath <png>`（按窗口句柄 `PrintWindow`，自带空白帧检测）。
3. 切页：QML 控件没有 HWND，只能靠输入消息。**实测踩坑**：`mouse_event` 的屏幕坐标与窗口原点不一致会点空；改用 `PostMessage(WM_LBUTTONDOWN/UP)`，坐标必须是**客户区物理像素**（125% DPI 即逻辑值 ×1.25）。导航项中心实测逻辑坐标：首页 (95,79)、配置 (95,125)、模块 (95,171)、关于 (95,217)。
4. 尺寸矩阵：默认 1280×800、最小 900×600（用 `MoveWindow` 缩窗）、宽屏 1600×1000。
5. 主题抽查：`light`、`night`、`charcoal_pink`（后者是 16 套里对比度问题最多者之一）。

### 8.2 数值复核

- 对比度：按 `ThemeManager::recompute()` 复现派生，跑 16 套 × 令牌组合，输出未达 4.5:1 清单（校验点：`light` surface/textPrimary = 7.10、`night` = 12.65，与 `docs/theme.md` 一致）。
- 令牌纪律：`grep` 页面内颜色/尺寸字面量，目标 0 处新增。
- 契约一致性：`grep` QML 内 `onClicked|onPressed|onXxx`，与 `src/ui/README.md` 例外表逐条比对，差集必须为空。

### 8.3 手工验收清单

| 项 | 动作 | 通过判据 |
| --- | --- | --- |
| P0-1 | 连 3 条线后拖动节点、平移画布 | 都能动，虚线消失 |
| P0-2 | 未连接设备看配置页按钮 | 明显灰化 |
| P0-3 | 上滚日志后等新日志 | 视图不跳回底部 |
| P0-4 | 12 条模块/规则 | 启停按钮仍可点 |
| P0-5 | 900×600 打开三个对话框 | 主按钮可见 |
| P0-6 | 改节点后按 Esc | 弹确认，取消后改动保留 |
| P0-7 | 断开设备后点启动 | 显示「已启动（无输出）」 |
| P0-8 | 手输强度、点关闭通道 | 不吞字、需确认 |

### 8.4 尚未验证（如实声明）

- 规则编辑器、波形编辑器、波形库三个对话框**没有实机画面**：本轮用 `PostMessage` 合成点击打开失败（按钮未响应），其中的问题全部来自源码审计，属"代码可证、画面未证"。
- `dataChanged` 聚合信号的整页重算代价（`HomeBridge.h:61` 一个信号覆盖 12 个属性）没有实测数据，动桥接层前建议先量一次绑定重算耗时。
- 只做了 125% DPI 一档，150% 与 100% 下的字号观感未验证。

---

## 九、附录：问题索引

| 级别 | 位置 | 一句话 | 批次 |
| --- | --- | --- | --- |
| P0 | `dialogs/RuleEditorDialog.qml:462`（+281/437/457） | 连线后 `connectFrom=0` 导致画布死锁 | 1 |
| P0 | `components/AppButton.qml:16-38`、`NavButton.qml:15-18` | 无禁用态无焦点态 | 0 |
| P0 | `pages/ConfigPage.qml:513` | 日志强制贴底 | 1 |
| P0 | `home/ChannelPanel.qml:23-274`、`pages/ModulePage.qml:69-149` | 无滚动无 clip，启停按钮被挤出 | 1 |
| P0 | `dialogs/RuleEditorDialog.qml:18-19`、`WaveEditorDialog.qml:13-14`、`WaveSelectDialog.qml:13-14` | 写死尺寸，最小窗口溢出 | 1 |
| P0 | `dialogs/RuleEditorDialog.qml:620-625`、`WaveEditorDialog.qml:312-316` | 未保存即关，无拦截 | 1 |
| P0 | `include/ui/DeviceController.h:51/88`、`HomeBridge.cpp:88-95/114-117` | 输出与错误状态不可见 | 2 |
| P0 | `ChannelPanel.qml:267-273`、`ConfigPage.qml:405-437`、`ChannelPanel.qml:65-69` | 启停/强度入口缺少约束 | 2 |
| P1 | `ThemeManager.cpp:260/267-269/271-272` | 组合色未做对比度校正，最低 3.03:1 | 3 |
| P1 | `style/Typography.qml:6-14` | 9 档无辨识间距，10px 承载状态数据 | 0 |
| P1 | `ConfigPage.qml:153-208/265/285/449/490/505` | 波形 B 预览塌陷、色块同色、日志卡高度不自洽 | 3 |
| P1 | `home/ChannelPanel.qml:55-80` | 强度三入口并列、A/B 布局不一致 | 3 |
| P1 | `ModulePage.qml:86/119-123`、`AboutPage.qml:34-40` | 禁用压暗按钮、elide 与 wrap 冲突、关于页缺许可证 | 4 |
| P1 | `RuleEditorDialog.qml:78/98-148/597/617/624` | 15 处信号处理器未登记，11 个 objectName 未进契约表 | 1 |
| P2 | `RuleEditorDialog.qml:412/470/602`、`RuleGraphBridge.h:67` 未用 | 右键菜单位置、缩放无指示、无法删连线 | 5 |
| P2 | `ConfigPage.qml:250-282`、`ModulePage.qml:131-136` | 裸 MouseArea 键盘不可达、死热区 | 5 |

---

## 十、实施进展

### 已落地（分支 `ui/p0-p1-foundation`）

| 提交 | 内容 | 验收证据 |
| --- | --- | --- |
| 方案文档 | 本文档 | — |
| `c4ce1ef` | 设计系统底座：Theme 新增 13 个颜色令牌、对比度改为 `recompute()` 内结构性保证、Typography 五档 + 过渡别名、9 个组件禁用/焦点态 | 独立构建通过；13 项对比度用独立公式复算与自报一致（差异 ≤0.06，属 16 位量化）；实机 0 告警 |
| `a549718` | 规则编辑器 `connectFrom` 死锁、三个对话框视口钳制、画布令牌化 | 构建通过；`connectFrom` 四处（默认 -1 / 三处守卫 / 复位 -1）一致；**手势与对话框画面未验证**（合成点击唤不起对话框） |
| `a549718` | 通道卡片与模块页溢出、强度输入吞字、蓝牙按钮 `enabled`、页面字面量与字号收敛 | 构建通过；实机 0 告警；紧凑导航下启停按钮留在卡内 |
| `6761ea9` | 设备与通道状态可见性（`lastError`/`outputReady`/`pairingStage`/`channelAState/B`/`statusLevel`） | 构建通过；实机截图证明四种状态渲染（错误红字、无输出警示、状态栏分级） |
| - | P0-3 日志贴底跟随 + 启动期错误回填（`6761ea9`） | 构建通过；探针日志证明上滚后 contentY 保持 280、回底后随新日志 370→388；顺带发现并修掉"模型整体替换导致 contentY 归零"的隐性缺陷 |
| - | A/B 对等：首页（`a549718`）、配置页（`a549718`） | 脚本量像素：首页三档差值 0/0/0px；配置页 0/0/1px（修复前 1195 vs 76）；内部区块逐元素对齐 |
| - | P0-6 确认机制（`6761ea9`）：`ConfirmDialog` + `ask_confirm` + 规则编辑器关闭拦截 + 关闭通道确认 | 探针日志：dirty 时 cancel→callback_called=0、accept→=1；dirty=false 直接关闭；通道未运行不弹、运行中弹 |
| - | Typography 过渡别名删除、README 契约与例外表按真实清册对齐 | `grep` 全仓 0 命中；真实 QML 处理器清册 27 处全部登记在案 |
| - | P1-1 页面骨架与导航（提交「页面骨架、导航与一次性反馈浮层」）：`PageHeader`、`Toast`、导航取消折叠、状态栏改常驻摘要 | 探针实测：窄窗导航宽 150 且文案完整、四页副标题非空、Toast 3 秒过期、状态栏摘要不被一次性消息污染 |
| - | P1-2 首页与配置页重排（提交「首页与配置页重排」）：`StatusBadge`、`AppButton.outline`、`AppSpinBox.showIndicator`、卡片内层去描边 | 探针实测：启停按钮 y=14（卡头）、`showIndicator=0` 仅作用于通道卡、ChannelPanel 内层描边 0 处、连接卡右列贴内容右缘（未分配空白 0）、日志列表高 270 |
| - | P1-3 反馈补全与模块/关于页（提交「反馈补全与模块/关于页收尾」）：`WaveBridge.lastError/draftDirty/draftSent`、`ModuleBridge.pluginToggled`、`LogBridge.lastExportPath` | 探针实测：空名保存给出具体原因、改正后清空；未连接设备发送报「未下发」、连接后报「已发送」；波形库删除取消 4→4 / 确认 4→3；插件失败带真实原因 |

### 与原始方案的偏差（已决策）

1. **P0-3 与 P0-6 从批次 1 移出**：日志贴底需要「用户是否上滚」的事件，关闭拦截需要确认动作；在「QML 零信号处理器」约束下两者都只能由 C++ 承担，放批次 1 会留下半成品。
2. **新增 4 个令牌**：`selectionText`（原方案要求 `accentText` 同时满足 `accent` 与 `selectionBg`，数学上不可满足，改为「底色成对 + 文字单独校正」）、`surfacePressed`、`dangerHover`（替换 `Qt.darker/lighter` 魔法值）、`transparentFill`（`ShapePath.fillColor` 默认是白色，不能直接删）。
3. **`graphGridSize` 20→40、`graphPortHitTolerance` 8→12**：前者让令牌对齐实现（否则下一个读代码的人会照令牌改实现），后者避免令牌化顺手把端口点击区从 12/10/14 缩小到 8。
4. **日志卡去掉写死的 `Layout.preferredHeight: 260`**：改为内容自适应（约 +14px），因为没有语义相符的令牌，且原值本身与内层 170 不自洽。

### 新发现（未修，待排期）

| 项 | 现象 | 归属 |
| --- | --- | --- |
| 窗口实际最小尺寸 | `MoveWindow` 压到 899×600 后布局回到约 1128×758，与 `Responsive.qml` 声明的 900×600 不符；P0-5 的真实作用区间是 1100 宽附近 | 批次 3 前核查（疑似 QML 隐式最小尺寸在顶） |
| 配置页通道 B 波形预览 | 实机仍被压成约 76px 窄条，A/B 不等宽 | 批次 3（5.5 配置页重构） |
| 启动期错误 | 已由 `UiConnector` 构造时用 `lastError()` 回填状态栏解决（`6761ea9`），`initialize()` 时序未动 | 已修 |
| **退出即崩溃** | 关闭主窗口后进程以 `0xC0000005`（访问冲突）结束，不是干净退出。`CloseMainWindow` 复现当前构建 3/3 次；对照改动前基线（`2728bd2`）构建同样复现 1/1 次 → **属既有缺陷，不是本次改动引入** | 需单独立项（不在 UI 方案内，建议单独排查退出/析构路径，注意该程序会拉起 Python 中转子进程） |
| 遮罩不透明度 | `ConfirmDialog.qml` 的 `opacity: 0.45` 仍是字面量，Metrics 无对应令牌 | 下次动令牌时补 `Metrics.scrimOpacity` |
| 剩余 27 处 QML 信号处理器 | 调色板 11 个按钮、模块列表 MouseArea、`ruleSaveButton`、检查器若干；已如实登记为例外，未迁移 | 后续批次按需迁 C++ |
| 无自动化测试 | 仓库无 tests 目标，全部验收靠构建 + 实机截图 | 长期 |

### 已确认可用的验证手法

- 构建：`cmake --build build --config Release --target DG-LAB-Client`（Visual Studio 2026 生成器，增量 1–3 分钟）。
- 实机冒烟：`./DG-LAB-Client.exe` 后台启动，8 秒后 `taskkill`。
  **注意：重定向 stdout/stderr 收不到这个 GUI 子系统程序的 Qt 消息**（实测 `> log 2>&1` 恒为 0 字节，`QML_IMPORT_TRACE=1` 对照仍为 0）。要证明"无 QML 告警"必须用临时 `qInstallMessageHandler` 把消息落盘再读，探针用完删除并重建。早期几条"日志 0 字节 = 无告警"的结论按此标准应打折。
- 截图：`cap.ps1` 按窗口句柄抓图（自带空白帧检测）；导航点击必须用 `PostMessage(WM_LBUTTONDOWN/UP)`，坐标是客户区物理像素（125% DPI 即逻辑 ×1.25）。**导航项中心随窗口宽度变化，不要照抄固定坐标**，先抓一张图定位。
