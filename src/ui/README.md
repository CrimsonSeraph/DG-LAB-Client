# ui（QML 界面层）

## 职责

提供全部界面（QML）与静态资源。界面**只通过属性绑定**读取 C++ 注入的桥接对象状态；所有交互（按钮点击、下拉切换、对话框确认、列表选中）由 C++ 侧按 `objectName` 显式建立连接，QML 中不写任何信号处理器。

明确不负责：业务规则（`rule/`）、数值调度与插件（`module/`）、设备通信（`DeviceController`）、配置解析（`core/`）。

## 依赖

| 依赖 | 说明 |
| --- | --- |
| `Qt6::Quick` / `Qt6::Qml` | QML 运行时 |
| `Qt6::QuickControls2` | 控件（应用统一样式设为 `Basic`，外观完全由样式令牌控制） |
| `Qt6::QuickDialogs2` | `ColorDialog`（自定义主题取色）等 |
| `AppConfig` / `DebugLog` | 核心基础设施 |
| `ModuleManager` / `RuleManager` | 数据与规则来源 |
| `DeviceController` / `DglabRelayServer` / `CoyoteBleController` | 设备通信：应用内 V3 / V4 中转服务与郊狼 V3 蓝牙直连 |

## 目录结构

```text
src/ui/
│   （QML 模块在根 CMakeLists.txt 中通过 qt_add_qml_module 声明）
├── README.md
├── qml/
│   ├── MainWindow.qml          # 左侧导航 + 内容区页栈 + 左下常驻面板 + 状态栏（原生标题栏；自绘标题栏与窗口控制按钮为后续任务）
│   ├── dialogs/
│   │   ├── RuleEditorDialog.qml # 规则可视化编辑器（画布手势例外见下）
│   │   ├── WaveSelectDialog.qml
│   │   └── WaveEditorDialog.qml
│   ├── pages/
│   │   ├── HomePage.qml        # 主页：A/B 通道面板（导航第 1 项）
│   │   ├── PluginCenterPage.qml # 插件中心（导航第 2 项，当前为骨架）
│   │   ├── WavePage.qml        # 波形设置（导航第 3 项，当前为骨架）
│   │   ├── MiniGamePage.qml    # 小游戏（导航第 4 项，当前为骨架）
│   │   ├── LogPage.qml         # 日志（导航第 5 项，当前为骨架）
│   │   ├── ConfigPage.qml      # 设置：连接 / 波形 / 主题 / 日志（仅经左下角面板入口可达）
│   │   ├── ModulePage.qml      # 模块页（不在导航中，仅经首页入口可达）
│   │   └── AboutPage.qml       # 关于（仅经左下角面板入口可达）
│   ├── home/
│   │   └── ChannelPanel.qml    # 单通道面板（强度 / 模块 / 规则 / 波形 / 启停）
│   ├── components/
│   │   ├── GlassCard.qml       # 卡片容器
│   │   ├── AppButton.qml       # 通用按钮（primary / danger / outline）
│   │   ├── NavButton.qml       # 左侧导航按钮
│   │   ├── AppSpinBox.qml      # 主题化 SpinBox
│   │   ├── AppTextField.qml    # 主题化单行输入框
│   │   ├── AppTextArea.qml     # 主题化多行输入框
│   │   ├── AppComboBox.qml     # 主题化下拉框
│   │   ├── AppCheckBox.qml     # 主题化勾选框
│   │   ├── AppSwitch.qml       # 主题化开关
│   │   ├── AppSlider.qml       # 连续值滑杆（可选数值标签）
│   │   ├── AppTabs.qml         # 标签页切换条
│   │   ├── AppChip.qml         # 可选中标签片
│   │   ├── AppCard.qml         # 横幅 / 标签 / 状态卡片
│   │   ├── AppField.qml        # 表单行（标签 + 提示 + 内容槽）
│   │   ├── AppSparkline.qml    # 迷你曲线
│   │   ├── AppStatusButton.qml # 带状态点的按钮
│   │   ├── AppSegment.qml      # 分段选择器
│   │   ├── ConfirmDialog.qml   # 通用确认弹层（ask_confirm 复用）
│   │   ├── PageHeader.qml      # 页面标题行（标题 + 副标题 + 右侧操作区）
│   │   ├── StatusBadge.qml     # 状态徽标（info / success / warning / danger）
│   │   ├── Toast.qml           # 右下角一次性反馈浮层
│   │   └── WavePreview.qml     # 波形预览（强度曲线 + 频率曲线）
│   └── style/                  # 【QML 单例】设计令牌（页面只引用，不写字面量）
│       ├── Metrics.qml         # 间距 / 圆角 / 描边 / 通用尺寸
│       ├── Typography.qml      # 字号
│       ├── Responsive.qml      # 断点与派生判断
│       └── ComponentStyle.qml  # 组件专属度量（通道卡片 / 波形预览 / 节点编辑器 / 色块）
└── （C++ 桥接见 include/ui、src/ui）
```

> 颜色令牌不在这里：主题需要支持运行时切换与自定义主/副色，因此由 C++ 的 `ThemeManager` 注册为 QML 单例 `Theme`（`qmlRegisterSingletonInstance`），暴露 `primary` / `secondary` / `surface` / `textPrimary` / `accent` 等语义化颜色。

## C++ 桥接对象

| 上下文属性 | 类型 | 职责 |
| --- | --- | --- |
| `app` | `AppBridge` | 应用名称/版本、当前页面（`currentPage`）、一次性反馈浮层（`toastText` / `toastLevel` / `showToast()`） |
| `device` | `DeviceController` | 内置中转服务状态、局域网地址/端口、配对链接 `pairingUrl` 与二维码 `qrImageUrl`，强度 / 波形 / 清除指令下发与设备回传转发 |
| `home` | `HomeBridge` | A/B 通道强度、上限、启用状态、模块摘要、规则摘要，以及启停与强度调整 |
| `moduleBridge` | `ModuleBridge` | 插件/内置模块卡片、统一查询周期、选中模块数值 |
| `waveBridge` | `WaveBridge` | 波形库、A/B 当前波形与预览点、波形编辑器草稿 |
| `ble` | `CoyoteBleController` | 郊狼 V3 蓝牙扫描/连接、电量、强度、波形播放；蓝牙连接后规则输出与波形发送优先经蓝牙直连（否则走内置中转服务） |
| `ruleGraph` | `RuleGraphBridge` | 规则图节点/连线、模块数值面板、节点编辑与保存写回 |
| `Theme` | `ThemeManager`（QML 单例） | 主题令牌（17 套预设 + 自定义主/副色），`presets` / `applyPreset` / `applyCustom`；令牌清单与预设色值见 [docs/theme.md](../../docs/theme.md) |
| `updateBridge` | `UpdateBridge` | 检查更新、下载更新包（进度 / 错误）、重启并安装；安装经 `confirmDialog` 确认 |

## objectName 契约表

> 导航重排移除了 `navConfigButton` / `navModuleButton` / `navAboutButton` 三个旧条目，其余条目未改。

导航与左下角面板：

左侧导航按「主页 / 插件中心 / 波形设置 / 小游戏 / 日志」排列；`ModulePage` 不在导航中，仅经首页入口可达，`AboutPage` 仅经左下角面板入口可达。导航项与面板入口的 `objectName` 全部由 `UiConnector` 的 `nav_bindings()` 映射表统一接管（objectName → `AppBridge::Page`），QML 不写 `onClicked`；面板内的设备连接状态为只读绑定（`ble.connected` / `ble.deviceName`），不产生交互。

| objectName | 行为 |
| --- | --- |
| `navHomeButton` | 主页（`AppBridge::HomePage`） |
| `navPluginCenterButton` | 插件中心（`PluginCenterPage`） |
| `navWaveButton` | 波形设置（`WavePage`） |
| `navMiniGameButton` | 小游戏（`MiniGamePage`） |
| `navLogButton` | 日志（`LogPage`） |
| `panelSettingsButton` / `panelAboutButton` | 左下角面板「设置」→ `ConfigPage`、「关于」→ `AboutPage` |
| `panelCheckUpdateButton` | 左下角面板「检查更新」→ 既有 `UpdateBridge::on_check_update_clicked()`；`checking` / `downloading` 期间置灰，结果由 Toast 回执 |
| `homeModuleEntryButton` | 跳转模块页 |
| `homeRuleEditorButton` / `configRuleEditorButton` | 打开规则编辑器 |

首页通道（`ChannelPanel`，`channel` 为 A/B）：

| objectName | 行为 |
| --- | --- |
| `channel{A,B}Card` | 通道卡根节点（`GlassCard`），用于几何度量 |
| `channel{A,B}StartButton` | 切换通道启用状态（同步规则引擎通道变量）；位于卡头右侧，运行态为 `danger` + `outline` |
| `channel{A,B}StatusBadge` | 状态徽标（`components/StatusBadge.qml`）：未运行 / 运行中 / 无输出，只展示不给交互 |
| `channel{A,B}StrengthSpin` | 设置目标强度（`valueModified`）；`showIndicator: false` |
| `channel{A,B}StrengthIncrease` / `Decrease` | 强度 ±1 |
| `channel{A,B}SelectWaveButton` | 选择波形 |

配置页：

| objectName | 行为 |
| --- | --- |
| `configConnectionCard` | 连接卡根节点（`GlassCard`），左列表单 + 右列二维码 |
| `configConnectionFormColumn` / `configQrColumn` | 连接卡左列表单列 / 右列二维码列（右列固定宽度） |
| `configIpField` / `configPortField` | 连接地址与端口（连接时读取并持久化） |
| `configConnectButton` | 连接 / 断开 |
| `configQrImage` | 连接成功后展示二维码 |
| `configThemeSelectButton` | 打开预设主题对话框 |
| `configThemeCustomButton` | 打开自定义主题对话框 |
| `themePresetList` / `themePresetClick` | 预设主题列表与行热区（`mode` 属性） |
| `customThemePrimaryButton` / `customThemeSecondaryButton` | 打开主/副色取色对话框 |
| `customThemeSaveButton` | 保存并应用自定义主题 |
| `configLogList` | 日志列表；`movementEnded` 时按 `atYEnd` 维护 QML 侧 `stickToBottom`（贴底跟随 / 上滚回看） |
| `bleScanButton` / `bleDisconnectButton` | 扫描 / 断开蓝牙设备 |
| `bleDeviceList` / `bleDeviceClick` | 蓝牙设备列表与行热区（`address` 属性） |
| `bleStrengthA{Increase,Decrease}` / `bleStrengthB{Increase,Decrease}` | 蓝牙通道强度增减 |

页面骨架与反馈：

| objectName | 行为 |
| --- | --- |
| `pageHeader` / `pageHeaderTitle` / `pageHeaderSubtitle` | `components/PageHeader.qml`：页面名 + 一行副标题 + 右侧操作区（默认属性槽）；副标题承载该页关键状态，四个页面各有同名实例 |
| `toast` / `toastText` | `components/Toast.qml`：右下角一次性反馈浮层，`app.showToast(text, level)` 驱动，3 秒后自动消失；描边按 `toastLevel` 取 `success` / `warning` / `danger` / `border` |
| `statusLabel` | 底部状态栏常驻摘要（中转 / 蓝牙 / 通道），**不**承载一次性消息 |
| `configWaveChannelAButton` / `configWaveChannelBButton` | 配置页选择波形目标通道 |
| `configWaveSelectButton` / `configWaveCreateButton` | 打开波形库 / 直接新建波形 |

模块页：

| objectName | 行为 |
| --- | --- |
| `modulePeriodCombo` | 统一查询周期下拉（`periodOptions`，文案为毫秒数值）；`activated` 与「应用」共用同一槽 |
| `modulePeriodApplyButton` | 把所选周期应用到全部数值 |
| `moduleCardFlow` | 模块卡片流式容器；委托热区由 `watch_items` 扫描 |
| `moduleCardClick` | 卡片热区（`moduleName` 属性，取模块标识而非展示名）：打开数值弹窗（仅已启用模块可点） |
| `pluginToggleClick` | 插件卡启用/禁用（`fileName` 属性）：成功不弹 Toast（徽标变化），失败 `ModuleBridge::pluginToggled` 弹 danger |
| `moduleStateBadge` | 卡片状态徽标（`StatusBadge`）：内置模块/已启用 level 1、未启用 level 0、加载失败 level 3 |
| `moduleEmptyStateText` | 模块列表空态文案（`moduleBridge.modules` 为空时可见）：未发现模块（检查 plugins 目录） |
| `moduleValuesDialog` | 选中模块的数值列表弹窗 |

关于页：

| objectName | 行为 |
| --- | --- |
| `aboutVersionText` / `aboutDescriptionText` | 版本号与一句话说明（同色阶 `Theme.textSecondary`） |
| `aboutLicenseText` | 许可证：GPL-3.0-only |
| `aboutDependenciesText` | 依赖：Qt 6 |
| `aboutRepositoryText` | 仓库地址 |
| `configCheckUpdateButton` | 关于页「软件更新」卡片内的旧入口（`objectName` 沿用 `config` 前缀属命名遗留）：检查更新入口已移至左下角常驻面板 `panelCheckUpdateButton`，关于页不再承载该入口，两侧共用 `UpdateBridge::on_check_update_clicked()`，关于页的卡片入口随该页重写移除 |
| `updateDownloadButton` | 下载更新包：`remote_version` 非空且未就绪时可见；`downloading` 期间置灰，进度由卡片内 ProgressBar 绑定 `progress` 展示 |
| `updateApplyButton` | 重启并安装：`ready_to_install` 时可见；由 `UiConnector::on_update_apply_clicked` 先经 `confirmDialog` 确认，确认后调用 `UpdateManager::apply_update_and_restart()` |

一次性反馈与常驻摘要分工：一次性结果文案一律走 `app.showToast()`；状态栏只显示 QML 侧组合的常驻摘要。

反馈分两档，避免「操作完成却没回执」与「明显变化还弹 Toast」两头都踩：

| 情况 | 处理 | 例子 |
| --- | --- | --- |
| 成功且界面本身有明显变化 | **不弹** Toast，靠界面变化回执 | 切换主题预设、启动/关闭通道、保存波形（编辑器状态行显示「已保存」）、启用插件（卡片徽标变化） |
| 成功但界面无变化 | 弹 Toast（`StatusInfo` / `StatusSuccess`） | 发送波形到通道（通道与秒数）、导出日志（完整路径）、保存自定义主题 |
| 失败 | **一律**弹 Toast（`StatusDanger`），界面上有就地位置时同时显示红字原因 | 波形保存/发送失败（状态行红字）、插件加载失败（卡片内红字 + Toast） |

对话框：

| objectName | 行为 |
| --- | --- |
| `ruleEditorDialog` | 规则编辑器；`ruleEditorCloseButton` 的关闭经 C++ 判断未保存改动后先弹确认 |
| `ruleCanvas` / `ruleZoomLabel` | 画布根项（C++ 通过它读写 `panX`/`panY`/`zoom`）与左下缩放百分比标签（纯绑定 `zoom`） |
| `ruleFitViewButton` | 适配视图：点击由 C++ 连接，按节点包围盒重置缩放与平移；无节点时回到 1.0 居中 |
| `ruleDeleteEdgeMenu` | 右键菜单「删除此连线」：仅当右键命中连线（`canvas.contextEdgeId > 0`）时可见，`onTriggered` 调用 `ruleGraph.deleteEdge()` |
| `confirmDialog` / `confirmAcceptButton` / `confirmCancelButton` | 通用确认弹层（`components/ConfirmDialog.qml`）：`UiConnector::ask_confirm()` 填入标题/正文/危险态并打开，确认执行挂起动作，取消只关闭 |
| `waveSelectDialog` / `waveLibraryList` | 波形库对话框与列表；`waveSelectCloseButton` 关闭 |
| `waveSelectClick` | 行热区（`waveName` 属性）：指派该波形到目标通道并关闭对话框；委托热区由 `watch_items` 扫描 |
| `waveDeleteClick` | 行内删除（`waveName` 属性，`AppButton.danger`）：先经 `confirmDialog` 确认不可恢复，确认后按名字删除 |
| `waveCurrentABadge` / `waveCurrentBBadge` | 行内「当前 A / 当前 B」徽标（`StatusBadge` level 0），按 `waveBridge.currentA/currentB` 显示 |
| `waveLibraryEmptyText` | 波形库空态文案，`waveBridge.waves` 为空时可见 |
| `waveSelectCreateButton` | 关闭波形库并新建波形（打开编辑器） |
| `waveEditorDialog` | 波形编辑器；`closePolicy: Popup.NoAutoClose`，关闭只走按钮，由 C++ 判断 `draftDirty` 后先弹确认 |
| `waveEditorCloseButton` | 关闭编辑器：草稿未保存时先弹确认，取消则对话框保持打开 |
| `waveEditorStatusLine` | 编辑器状态行：`waveBridge.lastError` 非空显示红字失败原因；否则 `draftDirty` 显示「草稿未保存」，已保存显示「已保存」 |
| `waveNameField` / `waveSaveButton` | 波形名称输入与保存；成败由 `lastError` + 状态行承担，保存成功不弹 Toast |
| `waveSendAButton` / `waveSendBButton` | 发送草稿到 A/B：`WaveBridge::draftSent` 按下发链路是否可用弹「已发送（通道与秒数）」或「未连接设备，波形未下发」 |
| `waveAddSectionButton` / `waveRemoveSectionButton` / `waveApplySectionButton` / `waveApplyRawButton` | 段落增删、应用段落参数、应用原始帧 |
| `waveSectionList` / `waveSectionClick` | 段落列表与行热区（`sectionIndex` 属性） |
| `waveRawFramesArea` | 原始 V3 帧文本域（每行一个 8 字节 HEX） |
| `waveSection{Duration,FreqStart,FreqEnd,StrengthStart,StrengthEnd}Spin` | 段落参数输入（`Binding` 回写选中段落） |

## 样式令牌（style/ 单例）

| 单例 | 职责 |
| --- | --- |
| `Metrics` | 间距刻度、圆角、描边、导航宽度、卡片内边距、按钮高度 |
| `Typography` | 五档字号：`fontBody` / `fontSubtitle` / `fontHeading` / `fontTitle` / `fontDisplay`，另加等宽字体族 `fontMonoFamily` |
| `Responsive` | 窗口断点（`isCompactNav` / `isNarrow`）与最小窗口尺寸 |
| `ComponentStyle` | 通道卡片、波形预览、规则节点编辑器、主题色块等组件专属度量 |
| `Theme`（C++） | 全部颜色令牌 |

新增样式时：颜色补进 `ThemeManager`，字号补进 `Typography`，通用间距/圆角补进 `Metrics`，断点补进 `Responsive`，组件度量补进 `ComponentStyle`，**不要**在页面里留字面量。

> `QT_QML_SINGLETON_TYPE` 是**逐文件**属性，一个 `set_source_files_properties()` 里写多组同名属性只有最后一组生效；因此 `CMakeLists.txt` 中一个单例一条调用。

## 输入控件约定

Qt 原生输入控件不随 `Theme` 变化，输入类控件统一使用 `components/` 下的封装：

| 控件 | 基于 | 说明 |
| --- | --- | --- |
| `AppSpinBox` | `SpinBox` | 内容区 / 上下指示器 / 背景全部取 `Theme` 令牌 |
| `AppTextField` | `TextField` | 文字 / 占位符 / 选区 / 背景主题化，聚焦描边转 `accent` |
| `AppTextArea` | `TextArea` | 同 `AppTextField`，用于表达式等长文本 |
| `AppComboBox` | `ComboBox` | 内容区 / 指示器 / 弹层 / 委托全部主题化 |
| `AppCheckBox` | `CheckBox` | 指示器与文字主题化，选中态用 `accent` |
| `AppButton` | `Button` | 三态：`primary` = 强调色实心、`danger` = 危险色实心、`outline` = 透明底 + 语义色描边与文字（可与 `danger` / `primary` 组合，默认 false 时视觉不变） |

- 控件只读 `Theme` / `Metrics` / `Typography` / `ComponentStyle`，不写任何颜色字面量。
- 新增输入控件先在此目录封装，尺寸常量补进 `ComponentStyle`，再替换页面中的原生控件。
- 主题令牌、预设配色与对比度约束见 [docs/theme.md](../../docs/theme.md)。

## 展示与容器组件

除输入控件外，`components/` 还提供以下展示/容器组件，同样只读令牌、不写信号处理器：

| 控件 | 基于 | 说明 |
| --- | --- | --- |
| `GlassCard` | `Rectangle` | 卡片容器：统一背景 / 圆角 / 描边 / 内边距，内容走默认属性 |
| `NavButton` | `Button` | 左侧导航按钮，`selected` 绑定选中态（不使用内部 `checked`） |
| `PageHeader` | `Item` | 页面标题行：标题 + 副标题 + 右侧操作区（默认属性槽） |
| `StatusBadge` | `Rectangle` | 状态徽标：level 0 信息 / 1 成功 / 2 警示 / 3 危险，纯展示 |
| `Toast` | `Rectangle` | 右下角一次性反馈浮层，绑定 `app.toastText` / `app.toastLevel` |
| `ConfirmDialog` | `Dialog` | 通用确认弹层，`closePolicy: NoAutoClose`，供 `ask_confirm` 复用 |
| `WavePreview` | `Item` | 波形预览：强度曲线（`accent`）+ 频率曲线（`secondary`），空态显示「无波形」 |

- 上述组件均无 `onXxx` / `Connections`，交互统一由 `UiConnector` 按 `objectName` 建立。
- 新增展示组件优先放入 `components/`，专属组件放页面目录；样式一律引用令牌。

### 组件库补充（骨架批次新增）

| 控件 | 用途 | 关键属性 / 信号 |
| --- | --- | --- |
| `AppSwitch` | 主题化开关（继承 `Switch`） | `checked`、`toggled()`（继承自 `Switch` / `AbstractButton`） |
| `AppSlider` | 连续值滑杆，可在右侧显示当前值 | `value` / `from` / `to`（`Slider` 别名）、`step`、`showValue`、`valueText`、`moved()` |
| `AppTabs` | 顶部标签页切换条（`Row` + `Repeater`） | `model`、`currentIndex`、`tabClicked(index)` |
| `AppChip` | 可选中标签片，用于筛选或状态片段 | `text`、`checked`、`clicked()` |
| `AppCard` | 带横幅 / 标题 / 副标题 / 标签 / 状态与操作区的卡片 | `title`、`subtitle`、`banner`、`tags`、`status`、默认属性槽 `actions`、`hasBanner`（由 `banner` 派生，决定横幅布局） |
| `AppField` | 表单行：标签 + 提示 + 内容槽 | `label`、`hint`、默认属性槽 `contentData` |
| `AppSparkline` | 迷你曲线，接受原始点或帧数据 | `points`、`frames`、`maxValue`、`showAxisLabels` |
| `AppStatusButton` | 带状态点的按钮（0 信息 / 1 成功 / 2 警示 / 3 危险） | `statusLevel`、`clicked()`（继承 `Button`） |
| `AppSegment` | 分段选择器 | `model`、`currentIndex`、`selected(index)` |

## 信号连接约定

- QML 不写 `onClicked` / `Connections` / `onXxx`；所有交互由 `UiConnector` 在 `engine.load()` 之后按 `objectName` 连接。
- 新增交互控件必须同步更新 `UiConnector` 与本文件的 objectName 表。
- 列表/中继器动态生成的委托（如 `themePresetList` 的 `themePresetClick`）不在 `QObject::children()` 中：`UiConnector::watch_items()` 沿 `QQuickItem::childItems()` 扫描热区并在 `childrenChanged` 后重扫。
- 用户可编辑但需要回灌的控件（如强度 `SpinBox`）使用 `Binding` 元素回写，避免用户操作破坏绑定。

桥接信号 → 反馈路由（`UiConnector` 构造期连接，QML 不参与）：

| 信号 | 反馈 |
| --- | --- |
| `WaveBridge::draftSent(channel, seconds)` | 先看下发链路 `device.outputReady`：可用 → Toast（info）`已发送到通道 A（3 秒）`；不可用 → Toast（warning）`通道 A 未连接设备，波形未下发`。`draftSent` 只代表调用链走完，不代表设备已收到 |
| `WaveBridge::draftChanged` + `lastError` 由空转非空 | Toast（danger）：失败原因；编辑器状态行同时显示红字 |
| `ModuleBridge::pluginToggled(file, ok, error)` | `ok` 不弹（徽标变化）；失败弹 Toast（danger，含文件名与原因） |
| `LogBridge::exportChanged` | Toast（info）：`日志已导出：<path>`，路径取 `lastExportPath` |
| `LogBridge::statusMessage` | 仅导出失败时发出 → Toast（danger） |
| `UpdateBridge::toast_requested(message, level)` | 直接映射为 `app.showToast(message, level)`：发现新版本 / 已是最新 / 下载完成 / 各类错误统一走 Toast，QML 不参与 |
| 自定义主题保存按钮 | Toast（success）：已保存并应用；预设主题切换不弹 |

新增「操作完成但界面无变化」的按钮时，按上表补一条桥接信号与 Toast，不要在 QML 里写处理器。

## 信号处理器例外（必须使用手势的视图）

默认规则不变：QML **不写** `onClicked` / `Connections` / `onXxx`。但**必须依赖指针手势或逐帧回调的视图**允许例外，且需逐处登记：

| 例外位置 | 原因 | 约束 |
| --- | --- | --- |
| `dialogs/RuleEditorDialog.qml` 画布 `MouseArea` 的 `onPressed` / `onPositionChanged` / `onReleased` / `onWheel` / `Keys.onPressed` | 拖动节点、拉线、缩放、平移与快捷键都是指针/键盘手势，命中测试无法通过 `objectName` + C++ 连接表达 | 处理器只调用 `ruleGraph`（C++）的方法；节点与连线的增删改、复制粘贴、写回全部在 C++，QML 不直接改模型 |
| 同文件画布右键菜单的 6 个 `MenuItem.onTriggered`（含 `ruleDeleteEdgeMenu` 删除连线） | 菜单由手势弹出，且需要读画布当前缩放/平移换算世界坐标；删除连线依赖右键命中测试得到的连线 id | 同上，只调用 `ruleGraph` |
| 同文件调色板 11 个 `AppButton.onClicked`（`ruleAddAdd` / `ruleAddSub` / `ruleAddMul` / `ruleAddDiv` / `ruleAddAbs` / `ruleAddSquare` / `ruleAddSqrt` / `ruleAddExpr` / `ruleAddRule` / `ruleAddChannelA` / `ruleAddChannelB`）与模块列表委托 `MouseArea.onClicked` | 遗留：这些点击都能用 `objectName` + `connect_clicked` 迁到 C++，目前尚未迁移 | 只调用 `ruleGraph`；新增控件不得照抄此写法 |
| 同文件检查器的 `onEditingFinished`（规则名 `nameField` / 表达式 `expressionArea`）、`onActivated`（值模式 `modeCombo`）、`onClicked`（启用勾选、应用表达式）、`ruleSaveButton.onClicked` | 遗留：输入回灌与保存沿用 QML 处理器 | 只调用 `ruleGraph`；不得扩大 |

已迁出：规则编辑器关闭按钮（`ruleEditorCloseButton`）原先写 `onClicked: dialog.close()`，现由 `UiConnector::on_rule_editor_close_clicked()` 连接，存在未保存改动时先经 `confirmDialog` 确认。

新增例外时必须同时更新本表，并在代码注释中写明"为何不能用 objectName + C++ 连接实现"。

复核口径：对 `src/ui/qml` 全量扫描 `onXxx:\b` / `Connections` / `Behavior`，命中 27 处且全部落在 `RuleEditorDialog.qml`，均属上表已登记条目；其余文件为 0，本批次未新增任何处理器。

## 扩展点与注意事项

- **新增页面**：在 `qml/pages/` 新增文件 → 加入 `CMakeLists.txt` 的 `QML_FILES` → 在 `MainWindow.qml` 的页面栈中追加 → 在 `AppBridge::Page` 与 `UiConnector` 中登记。
- **新增组件**：放入 `qml/components/`（通用）或对应页面目录（专属）；样式一律引用令牌。
- **命名约定**：QML 内部 id / 属性用 `camelCase`；`objectName` 面向 C++ 连接，必须与 `UiConnector` 中的字符串一致。
- **迁移状态**：界面已整体迁移到 QML；旧 Widgets 界面、`.ui` 文件与 `qcss` 样式表均已移除（见 [CHANGELOG.md](../../CHANGELOG.md)）。
