# ui（QML 界面层）

## 职责

提供全部界面（QML）与静态资源。界面**只通过属性绑定**读取 C++ 注入的桥接对象状态；所有交互（按钮点击、下拉切换、对话框确认、列表选中）由 C++ 侧按 `objectName` 显式建立连接，QML 中不写信号处理器（登记在案的例外见「信号处理器例外」一节）。

明确不负责：业务规则（`rule/`）、数值调度与插件（`module/`）、设备通信（`DeviceController`）、配置解析（`core/`）。

## 依赖

| 依赖 | 说明 |
| --- | --- |
| `Qt6::Quick` / `Qt6::Qml` | QML 运行时 |
| `Qt6::QuickControls2` | 控件（应用统一样式设为 `Basic`，外观完全由样式令牌控制） |
| `Qt6::QuickDialogs2` | `ColorDialog`（自定义主题取色）等 |
| `Qt6::Widgets` | `QSystemTrayIcon` / `QMenu`：系统托盘图标与三项菜单（`TrayController`） |
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
│   ├── MiniWindow.qml         # 迷你窗：无边框置顶小窗，A/B 强度大数字与占比、强度增减、通道启停、透明度与大小档位、近 5 条触发流水与 A/B 滚动波形
│   ├── dialogs/
│   │   ├── RuleEditorDialog.qml # 规则可视化编辑器（画布手势例外见下）
│   │   ├── UpdateDialog.qml     # 软件更新：版本 / 完整更新说明 / 进度 / 更新动作（左下角面板入口打开）
│   │   ├── WaveSelectDialog.qml
│   │   └── WaveEditorDialog.qml
│   ├── pages/
│   │   ├── HomePage.qml        # 主页看板：设备 / 通道 / 插件 / 触发流水（导航第 1 项）
│   │   ├── home/              # 看板卡片：设备状态、通道行与通道卡、已启用插件、触发流水
│   │   ├── PluginCenterPage.qml # 插件中心（导航第 2 项，当前为骨架）
│   │   ├── WavePage.qml        # 波形设置：目标通道、A/B 波形预览、波形库与编辑器入口（导航第 3 项）
│   │   ├── MiniGamePage.qml    # 小游戏（导航第 4 项，当前为骨架）
│   │   ├── LogPage.qml         # 日志：列表、级别过滤与手动导出选项（导航第 5 项）
│   │   ├── ConfigPage.qml      # 设置：连接（中转表单 + 扫码配对 + 蓝牙直连）与主题（仅经左下角面板入口可达）
│   │   ├── plugin/            # 插件中心：插件卡、插件详情、事件映射面板
│   │   └── AboutPage.qml       # 关于（仅经左下角面板入口可达）
│   ├── home/
│   │   └── ChannelPanel.qml    # 单通道面板（强度 / 模块 / 规则 / 波形 / 启停）；主页看板重写后已不再被引用，其 `channel{A,B}Card` / `channel{A,B}StatusBadge` / `channel{A,B}StrengthSpin` / `channel{A,B}StrengthDecrease` / `channel{A,B}StrengthIncrease` / `channel{A,B}SelectWaveButton` 均不在 `objectName` 契约内（`channel{A,B}StartButton` 由看板通道行沿用）
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
│   │   ├── AppDialog.qml       # 对话框基类（主题化底色 / 遮罩 / 标题行 / 视口内尺寸钳制）
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
| `moduleBridge` | `ModuleBridge` | 插件/内置模块卡片、统一查询周期（`periodOptions` / `basePeriodMs` / `uniformPeriodIndex`，写入口 `applyAllPeriod`）、选中模块数值 |
| `waveBridge` | `WaveBridge` | 波形库、A/B 当前波形与预览点、波形编辑器草稿 |
| `ble` | `CoyoteBleController` | 郊狼 V3 蓝牙扫描/连接、电量、强度、波形播放；蓝牙连接后规则输出与波形发送优先经蓝牙直连（否则走内置中转服务） |
| `logBridge` | `LogBridge` | 运行日志列表与界面级别过滤、手动导出（导出级别 / 范围 / 目录经 `setExportLevel` / `setExportRange` / `setExportDir` 写入 `user.json` 的 `app.log.manual.*`）、自动日志启动 |
| `ruleGraph` | `RuleGraphBridge` | 规则图节点/连线、模块数值面板、节点编辑与保存写回 |
| `Theme` | `ThemeManager`（QML 单例） | 主题令牌（17 套预设 + 自定义主/副色），`presets` / `applyPreset` / `applyCustom`；令牌清单与预设色值见 [docs/theme.md](../../docs/theme.md) |
| `updateBridge` | `UpdateBridge` | 检查更新、下载更新包（进度 / 错误）、重启并安装；安装经 `confirmDialog` 确认 |
| `tray` | `TrayController` | 系统托盘：图标与三项菜单（显示主窗口 / 显示迷你窗 / 退出）、双击图标恢复主窗口，以及主窗口关闭的落点（托盘可用则隐藏到托盘，不可用则退出） |

## objectName 契约表

> 历史条目 `navConfigButton` / `navModuleButton` / `navAboutButton` 已随导航重排移除；下表一律以代码中的实际 `objectName` 为准。

导航与左下角面板：

左侧导航按「主页 / 插件中心 / 波形设置 / 小游戏 / 日志」排列；模块页已从代码移除（首页入口现为「插件管理」→ 插件中心），`AboutPage` 仅经左下角面板入口可达。导航项与面板入口的 `objectName` 全部由 `UiConnector` 的 `nav_bindings()` 映射表统一接管（objectName → `AppBridge::Page`），QML 不写 `onClicked`；面板内的设备连接状态为只读绑定，口径与看板一致：蓝牙 `ble.connected` 计 0/1，加上 App 在线客户端数 `device.appClientCount`，合计即「已连接设备 ×N」，不产生交互。

| objectName | 行为 |
| --- | --- |
| `navHomeButton` | 主页（`AppBridge::HomePage`） |
| `navPluginCenterButton` | 插件中心（`PluginCenterPage`） |
| `navWaveButton` | 波形设置（`WavePage`） |
| `navMiniGameButton` | 小游戏（`MiniGamePage`） |
| `navLogButton` | 日志（`LogPage`） |
| `panelSettingsButton` / `panelAboutButton` | 左下角面板「设置」→ `ConfigPage`、「关于」→ `AboutPage` |
| `panelCheckUpdateButton` | 左下角面板「检查更新」→ `UiConnector::on_panel_check_update_clicked()`：先打开 `updateDialog`，再调用既有的 `UpdateBridge::on_check_update_clicked()`（同一入口，不复制检查逻辑）；`checking` / `downloading` 期间置灰，结果由 Toast 与对话框内状态回执 |
| `mainWindow` / `pageStack` / `statusBar` / `panelFooter` | 窗口根项、内容区页栈、底部状态栏、左下常驻面板容器 |
| `retiredModulePageSlot` | `pageStack` 中模块页移除后保留的空占位（保持页面索引与 `AppBridge::Page` 枚举一一对应），无交互 |
| `deviceStatusRow` / `deviceStatusDot` | 面板连接状态行与状态点（`connectionCount > 0` 时取 `success`） |
| `deviceStatusName` | 连接总数：`已连接设备 ×N`，N = 蓝牙 0/1 + App 在线客户端数（无连接时「未连接设备」） |
| `deviceStatusBle` / `deviceStatusApp` | 蓝牙 0/1（`蓝牙已连接` / `蓝牙未连接`）与 App 在线客户端数（`App 已连接 ×N` / `App 未连接`） |
| `deviceStatusDevice` | 蓝牙设备名：`ble.connected` 且 `ble.deviceName` 非空时可见 |
| `homePluginEntryButton` | 首页页头「插件管理」→ 插件中心（`UiConnector::on_home_plugin_entry_clicked()`） |
| `homeRuleEditorButton` / `configRuleEditorButton` | 打开规则编辑器 |

主页看板（`pages/HomePage.qml` 与 `pages/home/`）：

主页已改为看板：左列设备状态（内含 A/B 通道行）与 A/B 通道卡，右列已启用插件、近期触发流水与通道波形卡；页头只保留「插件管理」与「规则编辑器」两个快捷入口（模块页入口已随该页移除，波形设置入口已移除）。强度调节不在本页（后续由迷你窗提供），波形选择在波形设置页。

`homePage` / `homeScrollView` / `homeDashboardGrid` 为页面根项、滚动视图与两列看板栅格；`homeDashboardLeftColumn` / `homeDashboardRightColumn` 为左列（设备状态 + A/B 通道卡）与右列（插件 / 触发流水 / 通道波形）。

| objectName | 行为 |
| --- | --- |
| `dashboardDeviceCard` / `dashboardDeviceName` | 设备状态卡与卡头连接总数（`已连接设备 ×N` / `未连接设备`）：只读绑定 |
| `dashboardDeviceDot` / `dashboardDeviceState` / `dashboardDeviceAppState` | 状态点、蓝牙状态行（`蓝牙已连接` / `蓝牙未连接`）、App 状态行（`App 已连接 ×N` / `App 未连接`）：只读 |
| `dashboardDeviceBleName` / `dashboardOutputBadge` / `dashboardRelayState` | 蓝牙设备名、输出可用性徽标（`device.outputReady`）、中转摘要（`中转 已连接 · ip:port`）：只读 |
| `dashboardChannel{A,B}Card` | 通道卡根节点（`GlassCard`）：只读展示该通道已挂载模块与直连规则 |
| `dashboardChannel{A,B}Hint` | 通道卡脚注「强度调节与波形选择将在迷你窗与波形设置页提供」：只读 |
| `dashboardChannel{A,B}Name` / `dashboardChannel{A,B}Badge` | 通道状态行（`pages/home/DashboardChannelRow.qml`）的通道名与状态徽标（未运行 / 运行中 / 无输出）：只展示不给交互 |
| `channel{A,B}StartButton` | 切换通道启用状态（同步规则引擎通道变量）；位置已移到设备状态卡的通道行，沿用旧 `objectName`，运行态为 `danger` + `outline` |
| `dashboardPluginCard` / `dashboardPluginCount` | 已启用插件卡与其中的计数（`moduleBridge.modules` 中 `kind` 为 plugin 且已启用的项）；行热区点击跳转插件中心（`app.navigate`），该处理器尚未迁到 C++ |
| `dashboardTriggerCard` / `dashboardTriggerEmpty` | 近期触发流水卡（最近 8 条）与无记录时的空态文案：只读 |
| `dashboardWaveCard` / `dashboardWaveName{A,B}` | 通道波形卡与 A/B 当前波形名（`waveBridge.currentA/currentB`）；未绑定波形时名称行隐藏 |
| `dashboardWaveSparkline{A,B}` | A/B 强度曲线：取 `waveBridge.amplitudeA/amplitudeB`（每帧 4 个强度字节取均值，0~100），静态绘制 |
| `dashboardWaveStrength{A,B}` | A/B「强度 / 上限」数字行（`home.strength{A,B}` / `home.limit{A,B}`）：只读 |
| `dashboardWaveEmpty{A,B}` / `dashboardWaveHint` | 未绑定波形时的空态文案与卡底说明「静态预览 · 滚动播放在迷你窗提供」：只读 |
| `homeMiniWindowEntryButton` | 首页页头「迷你窗」入口 → `UiConnector::on_home_mini_window_clicked()`：显示迷你窗并置于前台 |

迷你窗（`MiniWindow.qml`：`Qt.Tool` + 无边框 + 置顶，不进任务栏；声明在 `MainWindow.qml` 内跟随应用生命周期）：

| objectName | 行为 |
| --- | --- |
| `miniWindow` | 迷你窗根项（`Window`）：启动时从 `app.ui.mini_window.{x,y}` 恢复位置并夹回可见屏幕，宽度取大小档位、高度按内容 `implicitHeight` 计算（封顶，滑杆与列表不撑高窗口）；位置 / 尺寸 / 透明度 / 大小档位变化后 400 ms 合并写回 `app.ui.mini_window.*`（`opacity` 缺省 1.0、`size` 缺省 `medium`） |
| `miniWindowFrame` / `miniWindowTitleBar` / `miniWindowTitleText` | 圆角底板（`Theme.surface` + `Theme.border`）、标题条与标题文字：只读；锁定态标题转 `Theme.textMuted` |
| `miniWindowLockButton` | 锁定 / 解锁（`UiConnector::on_mini_window_lock_clicked()` → `HomeBridge::toggleMiniWindowLock()`，写 `app.ui.mini_window.locked`）：锁定后拖动热区失效，按钮文案在「锁定 / 解锁」间切换 |
| `miniWindowCloseButton` | 关闭（×）只隐藏窗口并写回位置，不退出进程 |
| `miniWindowChannel{A,B}Name` / `miniWindowChannel{A,B}State` | 通道徽标与运行状态（`home.running{A,B}` → 运行中 / 未运行）：只读 |
| `miniWindowChannel{A,B}Value` / `miniWindowChannel{A,B}Limit` / `miniWindowChannel{A,B}Percent` | 强度大数字、上限（`home.strength{A,B}` / `home.limit{A,B}`）与占比（强度 / 上限，上限为 0 时显示 `--`）：只读 |
| `miniWindowStrength{A,B}Decrease` / `miniWindowStrength{A,B}Increase` | 强度增减：直接复用 `UiConnector::on_ble_strength_clicked()`——该槽按 objectName 里的 `StrengthA`/`StrengthB` 与 `Increase`/`Decrease` 判定通道与方向，迷你窗前缀不影响判定，调用链与配置页 `bleStrength{A,B}{Increase,Decrease}` 完全一致（`CoyoteBleController::setStrength()`）。显示的强度数字来源是设备回传，未连接设备时点击不产生可见变化 |
| `miniWindowChannel{A,B}ToggleButton` | 通道启停：直接复用看板的 `UiConnector::on_channel_toggle_clicked()`；`channel_of()` 改为大小写不敏感，`miniWindowChannelAToggleButton` 解析为 A，`channel{A,B}StartButton` 行为不变。关闭运行中的通道仍先经 `confirmDialog` 确认；按钮文案在「启动通道 / 关闭通道」间切换，运行态取 `danger` + `outline` |
| `miniWindowOpacitySlider` | 透明度（0.3~1.0，步进 0.05，右侧显示百分比）：拖动经 `UiConnector::on_mini_window_opacity_moved()` 写 `home.miniWindowOpacity`（`Window.opacity` 绑该属性即时生效），落盘走与几何同一套 400 ms 合并计时器，写 `app.ui.mini_window.opacity` |
| `miniWindowSizeSmallButton` / `miniWindowSizeMediumButton` / `miniWindowSizeLargeButton` | 大小三档（宽 280 / 340 / 420 逻辑像素）：`on_mini_window_size_clicked()` 按 `...SmallButton` 等后缀写 `home.miniWindowSize`（`small` / `medium` / `large`）；当前档位按钮取 `primary` 高亮，写 `app.ui.mini_window.size` |
| `miniWindowTriggerList` / `miniWindowTriggerEmpty` | 近 5 条触发流水：取 `eventBridge.triggers`（时间正序）倒序，字段与首页流水卡一致（时间 / 通道 / 模块·事件·子选项 / Δ）；无记录显示「暂无触发」；列表高度封顶 5 行，超出在列表内滚动 |
| `miniWindowWaveName{A,B}` / `miniWindowWaveSparkline{A,B}` / `miniWindowWaveEmpty{A,B}` | A/B 滚动波形：曲线取 `home.miniWindowWave{A,B}`（C++ 每 100 ms 把 `waveBridge.amplitude{A,B}` 的最近 32 帧推给桥接对象，循环播放），名称取 `waveBridge.current{A,B}`；未绑定波形时隐藏曲线与名称、显示「未绑定波形」 |

迷你窗的交互全部按上述 objectName 由 C++ 连接，`MiniWindow.qml` 不新增任何信号处理器（仍是标题条拖动 2 处登记例外）。波形滚动不在 QML 里开计时器：`UiConnector` 持有 100 ms 计时器，只随迷你窗可见性启停，每拍把 `WaveBridge::amplitude_a()` / `amplitude_b()`（每帧 4 个强度字节取均值，0~100，帧长 100 ms）按游标截取最近 32 帧写入 `HomeBridge::setMiniWindowWave()`。

主窗口关闭语义：`main.cpp` 设 `setQuitOnLastWindowClosed(false)`，主窗口 `onClosing` 只调 `tray.handleMainWindowClose()`——托盘可用时隐藏到托盘，托盘不可用时直接退出；退出只走托盘菜单的「退出」。

配置页：

| objectName | 行为 |
| --- | --- |
| `configScrollView` | 配置页滚动容器 |
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
| `themePresetDialog` / `customThemeDialog` | 预设主题对话框与自定义主题对话框（后者的主/副色与应用按钮即上两行的 `customTheme*` 控件） |
| `primaryColorDialog` / `secondaryColorDialog` | 主/副色取色对话框（`ColorDialog`），由 `customThemePrimaryButton` / `customThemeSecondaryButton` 打开 |
| `configBleColumn` | 连接卡内的蓝牙直连子区（位于扫码配对列下方，含状态 / 扫描 / 设备列表 / 强度增减） |
| `configBleStatusText` | 蓝牙直连状态行：已连接显示「已连接 <设备名>」，未连接只显示「未连接」（单一状态行，不重复） |
| `configBleStatusDetail` | 蓝牙状态明细（绑 `ble.status`）：与状态行文案相同时隐藏，扫描中 / 连接中 / 错误 / 断开等有价值的状态保留；只读 |
| `bleScanButton` / `bleDisconnectButton` | 扫描 / 断开蓝牙设备 |
| `bleDeviceList` / `bleDeviceClick` | 蓝牙设备列表与行热区（`address` 属性） |
| `bleStrengthA{Increase,Decrease}` / `bleStrengthB{Increase,Decrease}` | 蓝牙通道强度增减 |

波形设置页（`pages/WavePage.qml`）：外层 `ScrollView` 撑满窗口，页面只在窗口高度不足时滚动；波形内容全部来自配置页迁移，行为不变。

| objectName | 行为 |
| --- | --- |
| `waveScrollView` | 波形设置页滚动容器 |
| `waveChannelCard` | 通道波形卡根节点（`GlassCard`）：目标通道、A/B 预览与波形库入口 |
| `waveChannelAButton` / `waveChannelBButton` | 选择波形目标通道（对应 `waveBridge.targetChannel`） |
| `waveSelectButton` / `waveCreateButton` | 打开波形库 / 直接新建波形（打开 `waveEditorDialog`） |

日志页（`pages/LogPage.qml`）：页面用 `ColumnLayout` 撑满窗口高度，日志列表吃满面板剩余高度并自行滚动；手动导出设置写入 `user.json` 的 `app.log.manual.*`，导出时按该设置逐行过滤。

| objectName | 行为 |
| --- | --- |
| `logPanel` | 日志面板容器（最小高度取 `ComponentStyle.logCardHeight` 令牌） |
| `logList` | 日志列表；`movementEnded` 时按 `atYEnd` 维护 QML 侧 `stickToBottom`（贴底跟随 / 上滚回看） |
| `logLevelCombo` | 界面日志级别过滤（`logBridge.levelFilter` → `app.log.ui_log_level`） |
| `logExportButton` / `logClearButton` | 导出日志（成功弹 Toast 带完整路径）、清空列表 |
| `logExportLevelCombo` | 手动导出级别（`logBridge.exportLevel` → `app.log.manual.export_level`，DEBUG / INFO / WARN / ERROR） |
| `logExportRangeCombo` | 手动导出范围三态：0 仅该级别（`only_level=true`）、1 该级别及以上（`only_level=false, level_above=true`）、2 该级别及以下（`only_level=false, level_above=false`）；默认值取当前配置，不固定停在第 0 项 |
| `logExportDirField` / `logExportDirApplyButton` | 手动导出目录（`logBridge.exportDirConfig` 存 `app.log.manual.export_dir` 原文，可填相对程序目录的路径）；点「应用目录」创建目录并写入配置，失败弹 Toast |

页面骨架与反馈：

| objectName | 行为 |
| --- | --- |
| `pageHeader` / `pageHeaderTitle` / `pageHeaderSubtitle` | `components/PageHeader.qml`：页面名 + 一行副标题 + 右侧操作区（默认属性槽）；副标题承载该页关键状态，四个页面各有同名实例 |
| `toast` / `toastText` | `components/Toast.qml`：右下角一次性反馈浮层，`app.showToast(text, level)` 驱动，3 秒后自动消失；描边按 `toastLevel` 取 `success` / `warning` / `danger` / `border` |
| `statusLabel` | 底部状态栏常驻摘要（中转 / 蓝牙 / 通道），**不**承载一次性消息 |
| `homePage` / `pluginCenterPage` / `wavePage` / `miniGamePage` / `logPage` / `configPage` / `aboutPage` | 各页面根项标记，不承载交互 |
| `miniGamePagePlaceholder` | 小游戏骨架页的占位文案（该页尚未实现）：只读 |

插件中心（`pages/PluginCenterPage.qml`）：模块页已从代码移除——`pages/ModulePage.qml` 删除，`AppBridge::Page::ModulePage = 2` 与 `pageStack` 里的空占位 `retiredModulePageSlot` 只为保持页面索引与枚举对应。原来的统一查询周期控件随之迁到插件中心，其余模块页条目已随该页删除。

| objectName | 行为 |
| --- | --- |
| `modulePeriodCombo` | 统一查询周期下拉（`periodOptions`，文案为毫秒数值）；`currentIndex` 绑定 `moduleBridge.uniformPeriodIndex` 回显实际周期（全部数值一致时为该周期下标，不一致为 -1、无选中项，未注册数值时按 `basePeriodMs` 回显）；`activated` 与「应用」共用同一槽 |
| `modulePeriodMismatchHint` | 「各数值周期不一致」提示，仅 `uniformPeriodIndex < 0` 时可见：只读 |
| `modulePeriodApplyButton` | 把所选周期应用到全部数值 |

插件详情（`pages/plugin/PluginDetail.qml`）：头图区 + 三个页签（模式与输出 / 触发事件 / 数值）。页签切换沿用该文件既有的 `onTabClicked`（插件中心批次遗留，见「信号处理器例外」）；「数值」页签只读绑定 `moduleBridge.selectedValues`，选中哪个插件由 C++ 侧决定——详情根项 `item` 变化时 `UiConnector::on_plugin_detail_item_changed()` 调 `ModuleBridge::selectModule()`，QML 不写处理器。

| objectName | 行为 |
| --- | --- |
| `pluginDetail` | 详情根项；`item` 变化即同步模块标识到 `ModuleBridge::selectModule()`，其余属性只读 |
| `pluginValuesPanel` | 「数值」页签的滚动容器，`activeTab === 2` 时可见；`contentHeight` 取自 `valueColumn` 位置器，只读 |
| `pluginValuesScrollBar` | 数值列表竖向滚动条（`ScrollBar` 附属在 `pluginValuesPanel` 上，`policy: AsNeeded`），句柄取 `Theme` 令牌且常驻可见（默认 Basic 样式句柄静止时透明），无交互 |
| `pluginValuesEmpty` | 数值空态面板（「该插件尚未注册数值」），`selectedValues` 为空时可见，只读 |

关于页（`pages/AboutPage.qml`）：只保留软件介绍与许可证说明；「软件更新」卡片已整张移除，更新入口与更新状态全部移到左下角面板与 `dialogs/UpdateDialog.qml`，本页不再有更新按钮、进度条与说明文本。

| objectName | 行为 |
| --- | --- |
| `aboutVersionText` / `aboutDescriptionText` | 版本号与面向普通用户的中文介绍（说明这是什么软件、手机 App 扫码与蓝牙直连两种连接方式、规则与波形、游戏插件；与版本号同色阶 `Theme.textSecondary`）：只读 |
| `aboutLicenseText` | 许可证叙述：GPL-3.0-only |
| `aboutDependenciesText` | 第三方依赖叙述：Qt 6 |
| `aboutRepositoryText` | 仓库地址 |

一次性反馈与常驻摘要分工：一次性结果文案一律走 `app.showToast()`；状态栏只显示 QML 侧组合的常驻摘要。

反馈分两档，避免「操作完成却没回执」与「明显变化还弹 Toast」两头都踩：

| 情况 | 处理 | 例子 |
| --- | --- | --- |
| 成功且界面本身有明显变化 | **不弹** Toast，靠界面变化回执 | 切换主题预设、启动/关闭通道、保存波形（编辑器状态行显示「已保存」）、启用插件（卡片徽标变化） |
| 成功但界面无变化 | 弹 Toast（`StatusInfo` / `StatusSuccess`） | 发送波形到通道（通道与秒数）、导出日志（完整路径）、保存自定义主题 |
| 失败 | **一律**弹 Toast（`StatusDanger`），界面上有就地位置时同时显示红字原因 | 波形保存/发送失败（状态行红字）、插件加载失败（卡片内红字 + Toast） |

对话框：

下列 6 个对话框（`dialogs/` 下 4 个、`ConfirmDialog`、`ConfigPage` 内的预设主题与自定义主题）全部继承 `components/AppDialog.qml`，底色 / 描边 / 遮罩 / 标题行 / 尺寸钳制由基类统一提供；`title` 即标题行文本。

| objectName | 行为 |
| --- | --- |
| `ruleEditorDialog` | 规则编辑器；`ruleEditorCloseButton` 的关闭经 C++ 判断未保存改动后先弹确认 |
| `ruleGraphTitle` / `ruleBackToTopButton` | 标题栏图名（总图「总规则图」、插件图「<插件> 规则图」）与「返回总图」按钮（仅插件图下可见，点击 `ruleGraph.loadPluginGraph("")`，处理器尚未迁到 C++） |
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
| `updateDialog` | 软件更新对话框（`dialogs/UpdateDialog.qml`）：`modal`，可经 Esc / 点击遮罩 / 「关闭」收起；关闭只隐藏窗口，下载状态保存在 `UpdateBridge`，重开后继续显示进度 |
| `updateDialogVersionText` / `updateDialogUpdateBadge` | 当前版本行，以及「新版本 v<remote_version>」徽标（`hasUpdate` 时可见）：只读 |
| `updateDialogReleaseNotes` / `updateDialogReleaseNotesEmpty` | `release_notes` 全文（放在 `Flickable` 里，`contentHeight` 取内部 `Column` 位置器，不设最大行数也不 elide）与空说明占位文案：只读 |
| `updateDialogErrorText` | `error_message` 非空时以危险色显示错误行 |
| `updateDialogProgress` | 下载进度条：`downloading` 时可见，值取 `progress` |
| `updateDialogCheckButton` / `updateDialogDownloadButton` / `updateDialogApplyButton` | 检查更新 / 下载更新（`hasUpdate` 且未就绪时可见，下载中置灰）/ 重启并安装（`ready_to_install` 时可见，`danger` + `outline` 组合，沿用「先经 `confirmDialog` 确认」的既有槽） |
| `updateDialogReleaseUrlButton` / `updateDialogCloseButton` | 查看完整更新说明（`release_url` 非空时可见，调 `open_release_url()`）/ 关闭对话框（`UiConnector::on_update_dialog_close_clicked()`） |

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
| `AppDialog` | `Dialog` | 对话框基类：`surface` 底色 + `border` 描边、`Metrics.scrimOpacity` 模态遮罩、`spacingMd` 内边距、共用标题行（取 `title`，`fontSubtitle` + `textPrimary`）；理想尺寸用 `preferredWidth` / `preferredHeight`，实际尺寸按 `Overlay.overlay` 钳制，窗口缩小时不裁切 |
| `ConfirmDialog` | `AppDialog` | 通用确认弹层，`closePolicy: NoAutoClose`，供 `ask_confirm` 复用 |
| `WavePreview` | `Item` | 波形预览：强度曲线（`accent`）+ 频率曲线（`secondary`），空态显示「无波形」 |

- 上述组件均无 `onXxx` / `Connections`，交互统一由 `UiConnector` 按 `objectName` 建立。
- 新增展示组件优先放入 `components/`，专属组件放页面目录；样式一律引用令牌。
- **对话框一律继承 `components/AppDialog.qml`**：不要在对话框里各写一份 `background` / `Overlay.modal` / `width` 钳制。Basic 样式的 Dialog 默认底色不随主题变化（浅灰），只有走 `AppDialog` 才会取 `Theme.surface` / `Theme.border` 并随主题切换；`ConfigPage` 内的预设主题与自定义主题两个对话框同样继承它。

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
- 用户可编辑但需要回灌的控件（如波形段落参数 `SpinBox`）使用 `Binding` 元素回写，避免用户操作破坏绑定。

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

## 信号处理器例外

默认规则不变：QML **不写** `onClicked` / `Connections` / `onXxx`。例外只有三类：**必须依赖指针手势或逐帧回调的视图**、**必须由窗口事件决定的关闭语义**（两类都在下表逐处登记），以及插件中心批次遗留、尚待迁移的页面级处理器（见本节末尾复核口径，不属登记项）。

| 例外位置 | 原因 | 约束 |
| --- | --- | --- |
| `dialogs/RuleEditorDialog.qml` 画布 `MouseArea` 的 `onPressed` / `onPositionChanged` / `onReleased` / `onWheel` / `Keys.onPressed` | 拖动节点、拉线、缩放、平移与快捷键都是指针/键盘手势，命中测试无法通过 `objectName` + C++ 连接表达 | 处理器只调用 `ruleGraph`（C++）的方法；节点与连线的增删改、复制粘贴、写回全部在 C++，QML 不直接改模型 |
| 同文件画布右键菜单的 6 个 `MenuItem.onTriggered`（含 `ruleDeleteEdgeMenu` 删除连线） | 菜单由手势弹出，且需要读画布当前缩放/平移换算世界坐标；删除连线依赖右键命中测试得到的连线 id | 同上，只调用 `ruleGraph` |
| 同文件 14 个 `onClicked`：调色板 11 个 `AppButton`（`ruleAddAdd` / `ruleAddSub` / `ruleAddMul` / `ruleAddDiv` / `ruleAddAbs` / `ruleAddSquare` / `ruleAddSqrt` / `ruleAddExpr` / `ruleAddRule` / `ruleAddChannelA` / `ruleAddChannelB`）、插件输出与数值源两个列表委托的 `MouseArea`、标题栏 `ruleBackToTopButton` | 遗留：这些点击都能用 `objectName` + `connect_clicked` 迁到 C++，目前尚未迁移 | 只调用 `ruleGraph`（`addOperatorNode` / `addAdvancedNode` / `addRuleNode` / `addChannelNode` / `addPluginOutputNode` / `addModuleNode` / `loadPluginGraph`）；新增控件不得照抄此写法 |
| 同文件检查器 5 个：`onEditingFinished`（规则名 `nameField` / 表达式 `expressionArea`）、`onActivated`（值模式 `modeCombo`）、`onClicked`（启用勾选、「应用表达式」按钮） | 遗留：输入回灌与表达式应用沿用 QML 处理器；`ruleSaveButton` 已迁到 C++ | 只调用 `ruleGraph`；不得扩大 |
| `MiniWindow.qml` 标题条 `MouseArea` 的 `onPressed` / `onPositionChanged` | 无边框窗口的拖动只能由指针按下与位移驱动，指针轨迹无法用 `objectName` + C++ 连接表达 | 处理器只改 `Window.x/y`；锁定态由 `dragArea.enabled` 断掉拖动，位置写回配置全部在 C++（`UiConnector` 的 400 ms 合并计时器） |
| `MainWindow.qml` 的 `onClosing` | 窗口关闭是窗口事件，`close.accepted` 与「隐藏而不是销毁」必须在事件回调内决定 | 只调 `tray.handleMainWindowClose()`；隐藏 / 退出 / 降级判定全在 `TrayController` |

已迁出：规则编辑器关闭按钮（`ruleEditorCloseButton`）原先写 `onClicked: dialog.close()`，现由 `UiConnector::on_rule_editor_close_clicked()` 连接，存在未保存改动时先经 `confirmDialog` 确认。

新增例外时必须同时更新本表，并在代码注释中写明"为何不能用 objectName + C++ 连接实现"。

复核口径：对 `src/ui/qml` 全量扫描 `\b(on[A-Z][A-Za-z]*|Connections)\s*:`，命中 63 处，按文件分组：

- `dialogs/RuleEditorDialog.qml` 30 处：画布手势 5、右键菜单 6、左栏添加类 14、检查器 5，即上表四行，逐处已登记。
- `pages/PluginCenterPage.qml` 9 处、`pages/plugin/PluginDetail.qml` 8 处、`pages/plugin/EventMappingPanel.qml` 5 处、`pages/plugin/PluginCard.qml` 3 处：插件中心批次留下的页面级处理器（搜索框 `onTextChanged`、下拉 `onActivated` / 分段 `onSelected`、`SpinBox.onValueModified`、列表与卡片热区 `onClicked`、组件自定义信号 `onOpenDetail` / `onToggleEnabled` / `onBack`），尚未逐条登记，属待迁移项。
- `pages/home/DashboardPluginCard.qml` 1 处：插件卡行热区点击跳转插件中心，同上待迁移。
- `MiniWindow.qml` 2 处（标题条拖动 `onPressed` / `onPositionChanged`）与 `MainWindow.qml` 1 处（`onClosing`）：即上表新增两行，逐处已登记。
- `components/` 4 处（`AppTabs` / `AppSlider` / `AppSegment` / `AppChip` 各 1）：组件把自身鼠标事件转发成自己的信号（`tabClicked` / `moved` / `selected` / `clicked`），属组件内部实现，不是页面交互。

`Connections {` 与 `Behavior {` 全量为 0。

## 扩展点与注意事项

- **新增页面**：在 `qml/pages/` 新增文件 → 加入 `CMakeLists.txt` 的 `QML_FILES` → 在 `MainWindow.qml` 的页面栈中追加 → 在 `AppBridge::Page` 与 `UiConnector` 中登记。
- **新增组件**：放入 `qml/components/`（通用）或对应页面目录（专属）；样式一律引用令牌。
- **命名约定**：QML 内部 id / 属性用 `camelCase`；`objectName` 面向 C++ 连接，必须与 `UiConnector` 中的字符串一致。
- **迁移状态**：界面已整体迁移到 QML；旧 Widgets 界面、`.ui` 文件与 `qcss` 样式表均已移除（见 [CHANGELOG.md](../../CHANGELOG.md)）。
