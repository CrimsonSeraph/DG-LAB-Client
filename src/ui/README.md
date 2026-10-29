# ui（QML 界面层）

## 职责

提供全部界面（QML）与静态资源。界面**只通过属性绑定**读取 C++ 注入的桥接对象状态；所有交互（按钮点击、下拉切换、对话框确认、列表选中）由 C++ 侧按 `objectName` 显式建立连接，QML 中不写信号处理器（登记在案的例外见「信号处理器例外」一节）。

明确不负责：业务规则（`rule/`）、数值调度与插件（`module/`）、设备通信（`DeviceController`）、配置解析（`core/`）。

## 依赖

| 依赖 | 说明 |
| --- | --- |
| `Qt6::Quick` / `Qt6::Qml` | QML 运行时 |
| `Qt6::QuickControls2` | 控件（应用统一样式设为 `Basic`，外观完全由样式令牌控制） |
| `Qt6::QuickDialogs2` | `ColorDialog`（自定义主题取色）、`FileDialog`（波形导入导出的文件选择，`import QtQuick.Dialogs`；模块已在 `DGLAB_QT_MODULES` 中，无新增依赖） |
| `Qt6::Widgets` | `QSystemTrayIcon` / `QMenu`：系统托盘图标与三项菜单（`TrayController`） |
| `AppConfig` / `DebugLog` | 核心基础设施 |
| `ModuleManager` / `RuleManager` | 数据与规则来源 |
| `DeviceController` / `DglabRelayServer` / `IOutputDevice` / `CoyoteBleController` / `OpossumBleController` / `PawPrintsBleController` / `DeviceValueBridge` | 设备通信：应用内 V3 / V4 中转服务与蓝牙直连（郊狼 V3、负鼠 V1，以及爪印 V1.1 输入侧）；`DeviceController` 只依赖输出设备接口 `IOutputDevice`，经 `attach_output()` 可注入多台设备，默认设备评选为郊狼优先、其次负鼠；爪印是纯输入设备（不实现 `IOutputDevice`），通知经 `DeviceValueBridge` 写成 `paw` 模块数值 |

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
│   │   ├── IpSelectDialog.qml   # 网卡 IP 选择：黑/白名单维护 + 过滤后的候选地址（配置页入口打开）
│   │   ├── WaveSelectDialog.qml
│   │   ├── WaveEditorDialog.qml
│   │   ├── WaveExportDialog.qml   # 波形导出：格式（JSON / .pulse）与范围（当前波形 / 全部波形）
│   │   ├── WaveConflictDialog.qml # 导入名称冲突：覆盖 / 重命名 / 跳过，整批生效
│   │   └── WaveTagDialog.qml      # 波形标签选择器：勾选 / 新建 / 删除自定义标签
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
│   │   ├── AppMenu.qml         # 主题化菜单（底色 / 描边 / 圆角 / 内边距 + 项配色经 palette 下传）
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
| `device` | `DeviceController` | 内置中转服务状态、局域网地址/端口、二维码载荷 `pairingUrl`（官方终端二维码协议格式）、展示用短地址 `pairingUrlText` 与二维码 `qrImageUrl`，强度 / 波形 / 清除指令下发与设备回传转发（强度回执按当前生效链路二选一：蓝牙在线只认 BLE 的 B1 回执，中转回执只记录不外发，链路切换时按新链路重播一次，两条链路不会互相覆盖）；事件输出经 `set_event_output()` 与规则命令合并（取较大值、按通道上限钳制、相同值不重复下发）后下发；事件波形经 `apply_event_wave()` 每帧判定，`dataType=wave` 的波形切换后播放一次、播完恢复接管前的规则波形；`device.ipSelector`（`IpSelector`）暴露候选网卡 `candidates`、`blacklist` / `whitelist`、手动选中 `selectedIp` 与实际使用 `activeIp`，也是地址解析（手动选中优先 → 按名单自动 → 回退 `127.0.0.1`）的唯一来源；输出目标按通道单选，`outputTargets()` / `outputTarget(channel)` / `setOutputTarget(channel, target)` 读写 `app.output.<A|B>.target`（本版只有 `coyote` 一个可选值，界面不显示下拉，切换路径已就位）；默认设备与上下限：`outputDevices` 列出在线输出设备、`defaultDevice` / `defaultDeviceIndex` 给出评选结果、`setDefaultDeviceByIndex(index)` 写 `app.output.default_device`，上下限读取集中在 `limits_for(mac, channel)`（设备特有值 → `app.channels.<A|B>` → 内置默认 200/150），下发路径 `active_output()` 返回的就是默认设备 |
| `home` | `HomeBridge` | A/B 通道强度、上限、启用状态、模块摘要、规则摘要，以及启停与强度调整；默认设备口径另见 `outputDevices`（在线输出设备列表）/ `defaultDevice`（当前默认设备 MAC）/ `defaultDeviceIndex`（默认设备在列表中的下标，无默认设备为 -1）与写入口 `setDefaultDeviceIndex(index)`；上限取默认设备的 `min(limit, safety)`，切换默认设备后立即跟随；另带波形下发状态 `waveSending{A,B}`（是否正在下发）、`waveFrame{A,B}`（当前帧序号，1 起）、`waveFrames{A,B}`（总帧数，未下发时为 0），三者同随 `waveStateChanged` 广播 |
| `moduleBridge` | `ModuleBridge` | 插件/内置模块卡片、统一查询周期（`periodOptions` / `basePeriodMs` / `uniformPeriodIndex`，写入口 `applyAllPeriod`）、选中模块数值 |
| `waveBridge` | `WaveBridge` | 波形库（官方内置 16 个只读 + 用户波形；列表项带 `source` / `official` / `sendable` 与 `frameCountV2` / `frameCountV3`）、A/B 当前波形与预览点、波形编辑器草稿；`protocol` 读写 `app.wave.protocol`（v2 / v3，缺省 v3），发送按协议取 V2 / V3 帧，目标版本缺帧写 `lastError` 并拒发；`draftSendable` 给出当前协议下草稿是否可发送；导入导出支持 JSON / .pulse / zip（zip 只在范围「全部波形」时可选）；标签库分两层（内置标签是代码常量、不可删，自定义标签存 `config/waves/tags.json`），`tagLibrary` / `builtinTags` / `customTags` / `tagTargetName` / `tagMaxLength` 供波形页与标签选择器使用，删除自定义标签时把该标签从使用中的波形上一并移除；官方 16 个波形的标签是用户侧叠加数据（存同一文件的 `official_wave_tags` 段，重新加载官方资源后恢复），其帧与名称仍只读（波形数据结构与标签规则见 [docs/wave-data.md](../../docs/wave-data.md)） |
| `ble` | `CoyoteBleController` | 蓝牙扫描/连接入口（郊狼 V3 与负鼠 V1 共用一份扫描列表）：电量、强度、波形播放；已连接设备的蓝牙 MAC 与型号经 `deviceAddress` / `deviceModel` 暴露，作为默认设备配置键（`app.devices.<MAC>`、`app.output.default_device`）；蓝牙连接后规则输出与波形发送优先经蓝牙直连（否则走内置中转服务）。电量特征（0x1500）订阅通知并主动读取一次，两者回包都解析成百分比写入 `battery`，连接期间每 60 秒再读一次；重连后按最近一次绝对强度重发。扫描结果按蓝牙名 `47L1xxxx` 前缀分类（`kModelTable`），行上给出驱动支持程度三态（`support` / `supportText`，见下节）：郊狼 V3（`47L121`）为「已支持」；负鼠（`47L127`）为「实验性」但驱动已实现、可点选连接（行内提示「实验性驱动已实现，未经实机验证」）；爪印（`47L120`）/ 灵猫（`47L124`）与型号未知的非 `47L1` 设备为「实验性」且驱动尚未实现，点选只给出「<型号> 驱动尚未实现，本版无法连接」的提示；未登记的 `47L1` 前缀为「暂不支持」，行不可点击且 `connectDevice()` 同样拒绝，避免误连后写入不匹配的指令。连接状态、电量、强度与状态文案读的是当前生效链路（郊狼优先），负鼠屏幕强度同步不可用时由 `screenSyncAvailable` 给出 |
| `logBridge` | `LogBridge` | 运行日志列表与界面级别过滤、手动导出（导出级别 / 范围 / 目录经 `setExportLevel` / `setExportRange` / `setExportDir` 写入 `user.json` 的 `app.log.manual.*`）、自动日志启动 |
| `ruleGraph` | `RuleGraphBridge` | 规则图节点/连线、模块数值面板、节点编辑与保存写回 |
| `Theme` | `ThemeManager`（QML 单例） | 主题令牌（17 套预设 + 自定义主/副色），`presets` / `applyPreset` / `applyCustom`；令牌清单与预设色值见 [docs/theme.md](../../docs/theme.md) |
| `updateBridge` | `UpdateBridge` | 检查更新、下载更新包（进度 / 错误）、重启并安装；安装经 `confirmDialog` 确认 |
| `tray` | `TrayController` | 系统托盘：图标与三项菜单（显示主窗口 / 显示迷你窗 / 退出）、双击图标恢复主窗口，以及主窗口关闭的落点（托盘可用则隐藏到托盘，不可用则退出） |

图像提供器（由 `main.cpp` 注册到 `QQmlApplicationEngine`）：

| 协议 | 提供器 | 说明 |
| --- | --- | --- |
| `image://dglabqr/<rev>` | `QrImageProvider` | 配对二维码，由 `DeviceController` 持有；载荷为官方终端二维码协议格式（官网下载地址 + `DGLAB-SOCKET` + `ws://<ip>:<V3 端口>/<终端 ID>`），`qrcodegen` 按内容自动提升版本 |
| `image://modulebanner/<插件文件名>/<头图文件名>` | `ModuleBannerImageProvider` | 插件侧车头图：按 id 解析 `<插件扫描目录>/<dll 主干名>/<头图文件名>`（扫描目录取自 `ModuleManager::get_plugin_dir()`，不按程序目录另拼）；id 只允许「插件文件名 + 纯文件名」两段，含 `..`、盘符、路径分隔符或多余段一律拒绝；文件缺失或不是图片时返回空图，QML 侧保持空态 |

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

带插件标识的导航只有 `app.openPluginDetail(pluginKey)` 一个入口：`AppBridge` 先切到插件中心，再发 `pluginDetailRequested(pluginKey)`，`UiConnector::on_plugin_detail_requested()` 把键解析成插件文件名后写 `pluginCenterPage` 的 `selectedFile` 与 `detailVisible`（即打开该插件详情）。键可写插件文件名或模块标识，两者都匹配。目标插件未扫描到或已卸载时不打开详情：只停在插件中心列表并弹一次 `warning` Toast（`未找到插件 …，已打开插件中心`），不会留下空白详情。之所以不扩展 `navigate(int page)`：带键的语义只对插件中心成立，`navigate` 保持「纯切页」单一职责，其余 8 处调用点无需感知页面负载。

主页看板（`pages/HomePage.qml` 与 `pages/home/`）：

主页已改为看板：左列设备状态（内含 A/B 通道行）与 A/B 通道卡，右列已启用插件、近期触发流水与通道波形卡；页头只保留「插件管理」与「规则编辑器」两个快捷入口（模块页入口已随该页移除，波形设置入口已移除）。强度调节不在本页（后续由迷你窗提供），波形选择在波形设置页。

`homePage` / `homeScrollView` / `homeDashboardGrid` 为页面根项、滚动视图与两列看板栅格；`homeDashboardLeftColumn` / `homeDashboardRightColumn` 为左列（设备状态 + A/B 通道卡）与右列（插件 / 触发流水 / 通道波形）。

| objectName | 行为 |
| --- | --- |
| `dashboardDeviceCard` / `dashboardDeviceName` | 设备状态卡与卡头连接总数（`已连接设备 ×N` / `未连接设备`）：只读绑定 |
| `dashboardDeviceDot` / `dashboardDeviceState` / `dashboardDeviceAppState` | 状态点、蓝牙状态行（`蓝牙已连接` / `蓝牙未连接`）、App 状态行（`App 已连接 ×N` / `App 未连接`）：只读 |
| `dashboardDeviceBleName` / `dashboardOutputBadge` / `dashboardRelayState` | 蓝牙设备名、输出可用性徽标（`device.outputReady`）、中转摘要（`中转 已连接 · ip:port`）：只读 |
| `homeDefaultDeviceCombo` / `homeDefaultDeviceEmpty` | 默认设备下拉（`AppComboBox`，`model: home.outputDevices`、`textRole: "name"`、`currentIndex: home.defaultDeviceIndex`）：只列**在线输出设备**（爪印、灵猫是纯输入设备，不进列表），切换由 `UiConnector::on_home_default_device_activated(int)` → `HomeBridge::setDefaultDeviceIndex()` → `DeviceController::setDefaultDeviceByIndex()` 落到 `app.output.default_device`，随后强度、波形与事件波形的下发目标与上下限一起改用该设备；列表为空时下拉隐藏、显示 `homeDefaultDeviceEmpty` 的「无在线输出设备」 |
| `dashboardChannel{A,B}Card` | 通道卡根节点（`GlassCard`）：只读展示该通道已挂载模块与直连规则 |
| `dashboardChannel{A,B}Hint` | 通道卡脚注「强度调节与波形选择将在迷你窗与波形设置页提供」：只读 |
| `dashboardChannel{A,B}Name` / `dashboardChannel{A,B}Badge` | 通道状态行（`pages/home/DashboardChannelRow.qml`）的通道名与状态徽标（未运行 / 运行中 / 无输出）：只展示不给交互 |
| `channel{A,B}StartButton` | 切换通道启用状态（同步规则引擎通道变量）；位置已移到设备状态卡的通道行，沿用旧 `objectName`，运行态为 `danger` + `outline` |
| `dashboardPluginCard` / `dashboardPluginCount` | 已启用插件卡与其中的计数（`moduleBridge.modules` 中 `kind` 为 plugin 且已启用的项）；行热区点击经 `app.openPluginDetail(fileName)` 直达该插件详情（插件文件名缺失时退化为 `app.navigate(pluginCenterPage)` 只切页），该处理器尚未迁到 C++ |
| `dashboardTriggerCard` / `dashboardTriggerEmpty` | 近期触发流水卡（最近 8 条）与无记录时的空态文案：只读 |
| `dashboardWaveCard` / `dashboardWaveName{A,B}` | 通道波形卡与 A/B 当前波形名（`waveBridge.currentA/currentB`）；未绑定波形时名称行隐藏 |
| `dashboardWaveSparkline{A,B}` | A/B 强度曲线：取 `waveBridge.amplitudeA/amplitudeB`（每帧 4 个强度字节取均值，0~100），静态绘制 |
| `dashboardWaveStrength{A,B}` | A/B「强度 / 上限」数字行（`home.strength{A,B}` / `home.limit{A,B}`）：只读 |
| `dashboardWaveSending{A,B}` | A/B 下发指示行「正在下发 · 第 n/N 帧」（`home.waveSending{A,B}` / `home.waveFrame{A,B}` / `home.waveFrames{A,B}`）：正在下发时可见，停止或清除后自动隐藏 |
| `dashboardWaveEmpty{A,B}` / `dashboardWaveHint` | 未绑定波形时的空态文案与卡底说明「静态预览 · 强度与下发帧随通道实时刷新，滚动播放在迷你窗提供」：只读 |
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
| `configSelectIpButton` | 打开 `ipSelectDialog`（`UiConnector::on_config_select_ip_clicked()`）：先刷新候选并把当前地址填进 `pendingIp`；确定后写回 `configIpField` 并走既有 `setEndpoint()`，地址栏、绑定地址与二维码载荷不会分叉 |
| `configConnectButton` | 连接 / 断开 |
| `configQrImage` | 连接成功后展示二维码（载荷为 `device.pairingUrl`） |
| `configPairingUrlText` | 二维码下方的短地址文本（`device.pairingUrlText`，仅连接后可见）：完整二维码载荷过长，界面上只展示去掉官方前缀的 `ws://<ip>:<端口>/<终端 ID>` |
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
| `bleScanButton` / `bleDisconnectButton` | 扫描 / 断开蓝牙设备；断开按钮在郊狼或负鼠输出链路连接时可用，爪印（纯输入设备）连上后经 `ble.pawConnected` 单独放开 |
| `bleDeviceList` / `bleDeviceClick` | 蓝牙设备列表与行热区（`address` 属性）：行是 `Rectangle`（`Theme.surface` 底 + `Metrics.radiusSm` 圆角 + 常驻 `Theme.divider` 细边框），热区 `hoverEnabled: true`，悬停靠 `containsMouse` 绑定把底色换 `Theme.surfaceAlt`、描边换 `Theme.accent`，并给出手型光标；不新增任何 QML 信号处理器。行文字取 `Theme.textSecondary`（暂不支持的型号取 `Theme.textMuted`）并在名字后追加 C++ 给出的 `hint`；热区 `enabled: modelData.connectable`，光标随之在箭头与手型之间切换 |
| `bleDeviceSupportBadge` | 行尾的设备状态徽标（`StatusBadge`）：`modelData.support` 为 2 已支持（绿）/ 1 实验性（黄）/ 0 暂不支持（红），文案由 C++ 的 `modelData.supportText` 给出，QML 不判断型号 |

蓝牙型号三态与提示文案（全部由 `CoyoteBleController::identify()` 的 `kModelTable` 与 `on_device_discovered()` 决定，QML 只显示）：

| 型号 | 徽标 | 行内 hint | 可点选 |
| --- | --- | --- | --- |
| `47L121` 郊狼 V3 | 已支持 | 空 | 是 |
| `47L127` 负鼠 V1 | 实验性 | `型号 47L127，实验性驱动已实现，未经实机验证` | 是 |
| `47L120` 爪印 V1.1 | 实验性 | `型号 47L120，实验性驱动已实现，未经实机验证` | 是（首次连接成功弹一次「此设备连接为实验性，功能未经完整验证」） |
| `47L120100` 爪印 V1.0 | 暂不支持 | `需先升级固件（V1.0 不可连接，请在 DG-LAB 3.0 APP 更新配件固件）` | 否 |
| `47L124` 灵猫 V1 | 实验性 | `实验性驱动已实现，未经实机验证` | 是（首次连接成功弹一次「此设备连接为实验性，功能未经完整验证」） |
| 其它 `47L1` 前缀 | 暂不支持 | `不可连接（型号 xxx，型号未登记）` | 否 |

爪印连接成功后状态行显示「爪印已连接（实验性输入设备）」；50 指令未被 `51` 确认时显示「爪印未确认 50 指令」并弹一次 danger Toast。爪印不参与 `ble.connected` 的输出链路口径（该属性决定通道强度与默认设备），只体现在 `ble.status` 与型号行上。灵猫同为一类：连接中显示「正在连接灵猫…（需连按 5 次开机键才可被搜到）」，成功后显示「灵猫已连接（实验性输入设备）」，气压上报（首帧 D0）未确认时显示「灵猫未确认 50 指令」并弹一次 danger Toast；灵猫同样不进 `ble.connected` 与默认设备评选，本批没有新增 QML 信号处理器与提示控件。
| `bleStrengthA{Increase,Decrease}` / `bleStrengthB{Increase,Decrease}` | 蓝牙通道强度增减 |

波形设置页（`pages/WavePage.qml`）：外层 `ScrollView` 撑满窗口，页面只在窗口高度不足时滚动；波形内容全部来自配置页迁移，行为不变。

| objectName | 行为 |
| --- | --- |
| `waveScrollView` | 波形设置页滚动容器 |
| `waveChannelCard` | 通道波形卡根节点（`GlassCard`）：目标通道、A/B 预览与波形库入口 |
| `waveChannelAButton` / `waveChannelBButton` | 选择波形目标通道（对应 `waveBridge.targetChannel`） |
| `waveSelectButton` / `waveCreateButton` | 打开波形库 / 直接新建波形（打开 `waveEditorDialog`） |
| `waveImportButton` | 导入波形 → `on_wave_import_clicked()`：打开 `waveImportFileDialog`（`.json` / `.pulse` / `.zip` 按扩展名自动判格式，zip 解压后逐个 `.pulse` 复用同一套冲突三选与汇总） |
| `waveExportButton` | 导出波形 → `on_wave_export_clicked()`：打开 `waveExportDialog` 先选格式与范围 |
| `waveTagCard` | 波形标签卡根节点（`GlassCard`）：标签库（内置标签带锁标识、自定义标签用语义色，均为只读展示）与波形列表 |
| `waveTagWaveList` | 波形列表容器（`Repeater` 按 `waveBridge.waves` 生成）：每个条目的标签显示在波形名上方，无标签时该行不占位；本页不提供删除入口，删除在标签选择器里做 |
| `waveTagEditClick` | 条目内「编辑标签」（`AppButton`，`waveName` 属性）→ `on_wave_tag_edit_clicked()`：写 `WaveBridge::beginTagEdit` 并打开 `waveTagDialog`；官方波形条目同样可用，只读的是其帧与名称 |

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

已启用栏条目没有新增 objectName：「配置 →」热区同样调用 `app.openPluginDetail(fileName)`，走与首页看板相同的解析与降级路径；网格卡片的热区仍走页面内的 `openDetail()`（已在本页，无需再切页）。

插件详情（`pages/plugin/PluginDetail.qml`）：头图区 + 三个页签（模式与输出 / 触发事件 / 数值）。头图区按 `item.fileName` 与侧车 `banner` 拼 `image://modulebanner/<插件文件名>/<banner>`（`banner` 是纯文件名，带 `/` 或 `:` 时按无图处理），图片未就绪或加载失败时 `Image` 不显示，保持 `Theme.surfaceAlt` 铺底，不出现破图；图片上叠一层取 `Theme.windowBg` / `Theme.transparentFill` 的水平渐变遮罩，保证左侧标题可读。页签切换沿用该文件既有的 `onTabClicked`（插件中心批次遗留，见「信号处理器例外」）；「数值」页签只读绑定 `moduleBridge.selectedValues`，选中哪个插件由 C++ 侧决定——详情根项 `item` 变化时 `UiConnector::on_plugin_detail_item_changed()` 调 `ModuleBridge::selectModule()`，QML 不写处理器。「模式与输出」页签的「通道上限（未设设备特有值时的回退值）」面板一律读 `eventBridge.channels`（即 `app.channels.<A|B>` 的通道级 `limit` / `safety`），与当前插件是否声明事件无关，未启用或未声明事件的插件同样显示真实通道值；它是默认设备没有配置 `app.devices.<MAC>.{limit_a,safety_a,limit_b,safety_b}` 时的回退层，界面文案直接写明这一点，不存在第二套「全局上限」；插件未声明事件时只有「输出通道」下拉禁用并提示「该插件未声明事件，默认输出通道不可用」。

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

下列对话框（`dialogs/` 下 7 个、`ConfirmDialog`、`ConfigPage` 内的预设主题与自定义主题）全部继承 `components/AppDialog.qml`，底色 / 描边 / 遮罩 / 标题行 / 尺寸钳制由基类统一提供；`title` 即标题行文本。

| objectName | 行为 |
| --- | --- |
| `ruleEditorDialog` | 规则编辑器；`ruleEditorCloseButton` 的关闭经 C++ 判断未保存改动后先弹确认 |
| `ruleGraphTitle` / `ruleBackToTopButton` | 标题栏图名（总图「总规则图」、插件图「<插件> 规则图」）与「返回总图」按钮（仅插件图下可见，点击 `ruleGraph.loadPluginGraph("")`，处理器尚未迁到 C++） |
| `ruleCanvas` / `ruleZoomLabel` | 画布根项（C++ 通过它读写 `panX`/`panY`/`zoom`）与左下缩放百分比标签（纯绑定 `zoom`） |
| `ruleFitViewButton` | 适配视图：点击由 C++ 连接，按节点包围盒重置缩放与平移；无节点时回到 1.0 居中 |
| `ruleDeleteEdgeMenu` | 右键菜单「删除此连线」：仅当右键命中连线（`canvas.contextEdgeId > 0`）时才生成（`Repeater` 条件构建，未命中时不占菜单槽位），`onTriggered` 调用 `ruleGraph.deleteEdge()` |
| `confirmDialog` / `confirmAcceptButton` / `confirmCancelButton` | 通用确认弹层（`components/ConfirmDialog.qml`）：`UiConnector::ask_confirm()` 填入标题/正文/危险态并打开，确认执行挂起动作，取消只关闭；键盘由 `UiConnector::eventFilter` 在弹层可见时接管——Enter 确认、Esc 取消（弹层内出现输入框时 Enter 仍归输入），默认焦点按危险性分配：危险操作落在取消上、普通操作落在确认上，空白键只作用于默认焦点按钮；弹层是主窗口内的 `Popup`，主窗口隐藏到托盘或最小化时 `ask_confirm()` 先把主窗口 `show()` / `showNormal()` + `raise()` + `requestActivate()` 再打开，否则迷你窗触发的确认（如「关闭运行中的通道」）落在不可见的窗口里无法确认 |
| `ipSelectDialog` | 网卡 IP 选择对话框（`dialogs/IpSelectDialog.qml`）：左右两列维护黑/白名单、下方列出过滤后的候选网卡；`pendingIp` 属性保存待写入的地址，Esc / 点击遮罩走取消，Enter 走确定（`UiConnector::eventFilter`） |
| `ipBlacklistList` / `ipWhitelistList` | 黑名单 / 白名单列表，模型为 `device.ipSelector.blacklist` / `whitelist` |
| `ipBlacklistRemoveClick` / `ipWhitelistRemoveClick` | 行内「删除」按钮（`keyword` 属性，`AppButton.danger` + `outline`）：删除该关键词并刷新候选；委托热区由 `watch_items` 扫描 |
| `ipBlacklistField` / `ipWhitelistField` / `ipBlacklistAddButton` / `ipWhitelistAddButton` | 关键词输入框与「添加」按钮：空白关键词弹一次 `warning` Toast，重复关键词不写入 |
| `ipCandidateList` / `ipCandidateClick` | 候选网卡列表（网卡名 / 描述 / 地址）与行热区（`address` 属性）：点击把该地址写入 `dialog.pendingIp`，行描边转 `accent` 并显示「已选中」徽标 |
| `ipCandidateSelectedBadge` / `ipCandidateEmpty` | 选中态徽标与无可用网卡时的空态文案：只读 |
| `ipSelectActiveText` | 对话框内「当前使用：<activeIp>」提示行：只读绑定 `device.ipSelector.activeIp` |
| `ipSelectConfirmButton` / `ipSelectCancelButton` / `ipSelectAutoButton` | 确定（写回地址栏 + `app.websocket.ip` 并刷新二维码载荷，落 `app.network.selected_ip`）/ 取消（只关闭）/ 恢复自动选择（清掉手动选中、按名单重选一次并立即生效，对话框收起）；`确定` 在未选中时置灰 |
| `waveSelectDialog` / `waveLibraryList` | 波形库对话框与列表；`waveSelectCloseButton` 关闭 |
| `waveSelectClick` | 行热区（`waveName` 属性）：指派该波形到目标通道并关闭对话框；委托热区由 `watch_items` 扫描 |
| `waveDeleteClick` | 行内删除（`waveName` 属性，`AppButton.danger`）：先经 `confirmDialog` 确认不可恢复，确认后按名字删除；内置官方波形删除被 C++ 拒绝并弹 Toast（`「X」是内置官方波形，不可删除`） |
| `waveCurrentABadge` / `waveCurrentBBadge` | 行内「当前 A / 当前 B」徽标（`StatusBadge` level 0），按 `waveBridge.currentA/currentB` 显示 |
| `waveLibraryEmptyText` | 波形库空态文案，`waveBridge.waves` 为空时可见 |
| `waveSelectCreateButton` | 关闭波形库并新建波形（打开编辑器） |
| `waveExportDialog` | 波形导出对话框（`dialogs/WaveExportDialog.qml`）：先选格式（JSON / .pulse / zip）与范围（当前波形 / 全部波形），确认后由 C++ 打开系统保存对话框；`waveExportFormatSegment` / `waveExportScopeSegment` 的 `selected(index)` 由 `UiConnector` 接到 `WaveBridge::setExportFormat` / `setExportScope`，`currentIndex` 反向绑定 `exportFormatIndex` / `exportScopeIndex` |
| `waveExportHintText` / `waveExportConfirmButton` / `waveExportCancelButton` | 说明行（绑 `waveBridge.exportHint`，可导出时取 `textMuted`、不可导出时取 `warning`）、导出（`enabled: waveBridge.exportAllowed`，`.pulse` + 全部波形、zip + 当前波形等组合直接置灰并给出改选提示，绝不静默只导一个或多导）/ 取消 |
| `waveConflictDialog` | 导入名称冲突对话框（`dialogs/WaveConflictDialog.qml`，`closePolicy: Popup.NoAutoClose`）：标题下列出与库重名的名称；`on_wave_conflict_requested()` 打开前把建议名称写进输入框 |
| `waveConflictRenameField` | 重命名用的新名称输入框：`enabled: waveBridge.conflictCount === 1`（多条冲突时按序号自动生成），内容由 C++ 在点「重命名」时读取 |
| `waveConflictOverwriteButton` / `waveConflictRenameButton` / `waveConflictSkipButton` | 覆盖（`danger` + `outline`，同名官方波形仍会被拒绝并记原因）/ 重命名（`primary`）/ 跳过：选择对整批生效，结果走 `importFinished` 汇总 Toast |
| `waveImportFileDialog` / `waveExportFileDialog` | 系统文件对话框（`MainWindow.qml` 内的 `FileDialog`，`import QtQuick.Dialogs`）：打开 / 保存模式，只负责挑路径，读写全部由 `WaveLibrary` 完成；导入侧过滤器含 `.json` / `.pulse` / `.zip`，导出侧按当前格式切换过滤器与默认后缀（json / pulse / zip）；保存对话框打开前由 C++ 把建议路径写进 `currentFile`，`accepted` 经 `on_wave_import_file_accepted()` / `on_wave_export_file_accepted()` 交给 `WaveBridge`（`exportToFile` 会先去掉已知的波形扩展名再补目标后缀，避免保存框补错后缀） |
| `waveTagDialog` | 波形标签选择器（`dialogs/WaveTagDialog.qml`）：标题带波形名，列出内置标签（锁标识 + 「内置」徽标，不可删）与自定义标签（`danger` + `outline` 的删除按钮）；勾选即写入该波形，取消勾选只从波形上移除、不动标签库；Esc 经 `UiConnector::eventFilter` 收起（等同「完成」） |
| `waveTagDialogList` / `waveTagToggleClick` / `waveTagDeleteClick` | 标签列表、行热区（`tagName` 属性，勾选 / 取消）与行内删除（`tagName` 属性）：删除仍有波形在用的标签先经 `confirmDialog` 确认「N 个波形正在使用该标签」，确认后连同这些波形上的该标签一起移除 |
| `waveTagDialogNewField` / `waveTagDialogCreateButton` | 新建标签输入框（占位提示带 `tagMaxLength`）与「新建并勾选」→ `on_wave_tag_create_clicked()`：名字大小写不同即沿用库里已有写法，空名字忽略，超长与写入失败经 `operationFailed` 弹 danger Toast |
| `waveTagDialogCloseButton` | 「完成」：关闭对话框并调用 `WaveBridge::endTagEdit()` 清掉编辑目标 |
| `waveEditorDialog` | 波形编辑器；`closePolicy: Popup.NoAutoClose`，关闭只走按钮，由 C++ 判断 `draftDirty` 后先弹确认 |
| `waveEditorCloseButton` | 关闭编辑器：草稿未保存时先弹确认，取消则对话框保持打开 |
| `waveEditorStatusLine` | 编辑器状态行：`waveBridge.lastError` 非空显示红字失败原因；否则 `draftDirty` 显示「草稿未保存」，已保存显示「已保存」 |
| `waveNameField` / `waveSaveButton` | 波形名称输入与保存；成败由 `lastError` + 状态行承担，保存成功不弹 Toast |
| `waveSendAButton` / `waveSendBButton` | 发送草稿到 A/B：`WaveBridge::draftSent` 按下发链路是否可用弹「已发送（通道与秒数）」或「未连接设备，波形未下发」；当前协议（`app.wave.protocol`）下草稿缺帧时不发送，经 `lastError` 弹 danger Toast |
| `waveAddSectionButton` / `waveRemoveSectionButton` / `waveApplySectionButton` / `waveApplyRawButton` | 段落增删、应用段落参数、应用原始帧 |
| `waveSectionList` / `waveSectionClick` | 段落列表与行热区（`sectionIndex` 属性） |
| `waveRawFramesArea` | 原始 V3 帧文本域（每行一个 8 字节 HEX；空白、逗号、分号、`:`、`-`、`_` 分隔与 `0x` 前缀会被去掉，非法帧丢弃） |
| `waveSection{Duration,FreqStart,FreqEnd,StrengthStart,StrengthEnd}Spin` | 段落参数输入（`Binding` 回写选中段落） |
| `updateDialog` | 软件更新对话框（`dialogs/UpdateDialog.qml`）：`modal`，可经 Esc / 点击遮罩 / 「关闭」收起；关闭只隐藏窗口，下载状态保存在 `UpdateBridge`，重开后继续显示进度 |
| `updateDialogVersionText` / `updateDialogUpdateBadge` | 当前版本行，以及「新版本 v<remote_version>」徽标（`hasUpdate` 时可见）：只读 |
| `updateDialogReleaseNotes` / `updateDialogReleaseNotesEmpty` | `release_notes` 全文（放在 `Flickable` 里，`contentHeight` 取内部 `Column` 位置器，不设最大行数也不 elide）与空说明占位文案：只读 |
| `updateDialogNotesScrollBar` | 更新说明的竖向滚动条（`ScrollBar` 附属在同一 `Flickable` 上，`policy: AsNeeded`），句柄取 `Theme.textSecondary` / 按下 `Theme.accent`、宽度与圆角取 `Metrics`（默认 Basic 样式句柄静止时透明），无交互 |
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
| `AppMenu` | `Menu` | 主题化菜单：底色 / 描边 / 圆角 / 内边距取 `Theme` 与 `Metrics`；项文字色、高亮色、按下色与分隔线经 `palette`（`windowText` / `light` / `midlight` / `mid`）下传，声明式 `MenuItem` 与 `MenuSeparator` 无需逐项改样式。`Menu.delegate` 只用于 `Action`，不参与声明式菜单项，故项目走 palette 而不是自建委托 |
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
| `AppCard` | 带横幅 / 标题 / 副标题 / 标签 / 状态与操作区的卡片 | `title`、`subtitle`、`banner`、`tags`、`status`、默认属性槽 `actions`、`hasBanner`（`banner` 非空）、`bannerReady`（`banner` 非空且 `Image` 真正 `Ready`，决定是否占头图槽与左右对齐；坏图或加载中退回无图的居中态，不留空白头图槽） |
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
| `WaveBridge::importConflictRequested(count)` | 打开 `waveConflictDialog` 并写入建议名称，不弹 Toast |
| `WaveBridge::importFinished(summary, clean)` | Toast（info / warning）：一条汇总文案，含跳过与缺帧原因摘要 |
| `WaveBridge::exportFinished(summary)` / `WaveBridge::operationFailed(message)` | Toast（info）导出路径与缺帧说明 / Toast（danger）导入导出、标签新建与标签删除的失败原因 |
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
- `pages/home/DashboardPluginCard.qml` 1 处：插件卡行热区点击（行委托由 `Repeater` 动态生成，本次仍保留处理器）只调 `app.openPluginDetail(fileName)`，导航与定位全在 C++（`AppBridge` + `UiConnector::on_plugin_detail_requested()`），QML 不再自己决定导航目标；处理器本身仍属待迁移项。
- `MiniWindow.qml` 2 处（标题条拖动 `onPressed` / `onPositionChanged`）与 `MainWindow.qml` 1 处（`onClosing`）：即上表新增两行，逐处已登记。
- `components/` 4 处（`AppTabs` / `AppSlider` / `AppSegment` / `AppChip` 各 1）：组件把自身鼠标事件转发成自己的信号（`tabClicked` / `moved` / `selected` / `clicked`），属组件内部实现，不是页面交互。

`Connections {` 与 `Behavior {` 全量为 0。

## 扩展点与注意事项

- **新增页面**：在 `qml/pages/` 新增文件 → 加入 `CMakeLists.txt` 的 `QML_FILES` → 在 `MainWindow.qml` 的页面栈中追加 → 在 `AppBridge::Page` 与 `UiConnector` 中登记。
- **新增组件**：放入 `qml/components/`（通用）或对应页面目录（专属）；样式一律引用令牌。
- **命名约定**：QML 内部 id / 属性用 `camelCase`；`objectName` 面向 C++ 连接，必须与 `UiConnector` 中的字符串一致。
- **迁移状态**：界面已整体迁移到 QML；旧 Widgets 界面、`.ui` 文件与 `qcss` 样式表均已移除（见 [CHANGELOG.md](../../CHANGELOG.md)）。
