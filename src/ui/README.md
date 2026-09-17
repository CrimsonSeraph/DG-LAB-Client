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
| `PythonSubprocessManager` | 当前设备通信通道（阶段 7 将替换为应用内 WebSocket 服务） |

## 目录结构

```text
src/ui/
│   （QML 模块在根 CMakeLists.txt 中通过 qt_add_qml_module 声明）
├── README.md
├── qml/
│   ├── MainWindow.qml          # 左侧导航 + 页面栈 + 状态栏（原生标题栏）
│   ├── pages/
│   │   ├── HomePage.qml        # 首页：A/B 通道面板
│   │   ├── ConfigPage.qml      # 配置：连接 / 波形 / 主题 / 日志
│   │   ├── ModulePage.qml      # 模块页（阶段 4）
│   │   └── AboutPage.qml       # 关于
│   ├── home/
│   │   └── ChannelPanel.qml    # 单通道面板（强度 / 模块 / 规则 / 波形 / 启停）
│   ├── components/
│   │   ├── GlassCard.qml       # 卡片容器
│   │   ├── AppButton.qml       # 通用按钮
│   │   └── NavButton.qml       # 左侧导航按钮
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
| `app` | `AppBridge` | 应用名称/版本、当前页面（`currentPage`）、状态栏文本 |
| `device` | `DeviceController` | 内置中转服务状态、局域网地址/端口、配对链接 `pairingUrl` 与二维码 `qrImageUrl`，强度 / 波形 / 清除指令下发与设备回传转发 |
| `home` | `HomeBridge` | A/B 通道强度、上限、启用状态、模块摘要、规则摘要，以及启停与强度调整 |
| `Theme` | `ThemeManager`（QML 单例） | 主题令牌（14 套预设 + 自定义主/副色），`presets` / `applyPreset` / `applyCustom` |

## objectName 契约表

导航：

| objectName | 行为 |
| --- | --- |
| `navHomeButton` / `navConfigButton` / `navModuleButton` / `navAboutButton` | 切换页面 |
| `homeModuleEntryButton` | 跳转模块页 |
| `homeRuleEditorButton` / `configRuleEditorButton` | 打开规则编辑器（阶段 9） |

首页通道（`ChannelPanel`，`channel` 为 A/B）：

| objectName | 行为 |
| --- | --- |
| `channel{A,B}StartButton` | 切换通道启用状态（同步规则引擎通道变量） |
| `channel{A,B}StrengthSpin` | 设置目标强度（`valueModified`） |
| `channel{A,B}StrengthIncrease` / `Decrease` | 强度 ±1 |
| `channel{A,B}SelectWaveButton` | 选择波形（阶段 6） |

配置页：

| objectName | 行为 |
| --- | --- |
| `configIpField` / `configPortField` | 连接地址与端口（连接时读取并持久化） |
| `configConnectButton` | 连接 / 断开 |
| `configQrImage` | 连接成功后展示二维码 |
| `configThemeSelectButton` | 打开预设主题对话框 |
| `configThemeCustomButton` | 打开自定义主题对话框 |
| `themePresetList` / `themePresetClick` | 预设主题列表与行热区（`mode` 属性） |
| `customThemePrimaryButton` / `customThemeSecondaryButton` | 打开主/副色取色对话框 |
| `customThemeSaveButton` | 保存并应用自定义主题 |

## 样式令牌（style/ 单例）

| 单例 | 职责 |
| --- | --- |
| `Metrics` | 间距刻度、圆角、描边、导航宽度、卡片内边距、按钮高度 |
| `Typography` | `fontTiny` ~ `fontDisplay` |
| `Responsive` | 窗口断点（`isCompactNav` / `isNarrow`）与最小窗口尺寸 |
| `ComponentStyle` | 通道卡片、波形预览、规则节点编辑器、主题色块等组件专属度量 |
| `Theme`（C++） | 全部颜色令牌 |

新增样式时：颜色补进 `ThemeManager`，字号补进 `Typography`，通用间距/圆角补进 `Metrics`，断点补进 `Responsive`，组件度量补进 `ComponentStyle`，**不要**在页面里留字面量。

> `QT_QML_SINGLETON_TYPE` 是**逐文件**属性，一个 `set_source_files_properties()` 里写多组同名属性只有最后一组生效；因此 `CMakeLists.txt` 中一个单例一条调用。

## 信号连接约定

- QML 不写 `onClicked` / `Connections` / `onXxx`；所有交互由 `UiConnector` 在 `engine.load()` 之后按 `objectName` 连接。
- 新增交互控件必须同步更新 `UiConnector` 与本文件的 objectName 表。
- 列表/中继器动态生成的委托（如 `themePresetList` 的 `themePresetClick`）不在 `QObject::children()` 中：`UiConnector::watch_list()` 沿 `QQuickItem::childItems()` 扫描热区并在 `childrenChanged` 后重扫。
- 用户可编辑但需要回灌的控件（如强度 `SpinBox`）使用 `Binding` 元素回写，避免用户操作破坏绑定。

## 信号处理器例外（必须使用手势的视图）

默认规则不变：QML **不写** `onClicked` / `Connections` / `onXxx`。但**必须依赖指针手势或逐帧回调的视图**允许例外，且需逐处登记：

| 例外位置 | 原因 | 约束 |
| --- | --- | --- |
| `qml/ruleeditor/` 画布与节点（拖动、滚轮缩放、右键菜单、框选） | 手势无法通过 `objectName` + C++ 连接表达 | 处理器内只调用 C++ 桥接对象的方法/读取属性；节点与连线的增删改、复制粘贴、求值全部在 C++，QML 不直接改模型 |
| `Shape`/`Canvas` 的 `onPaint` 等绘制回调 | 渲染框架要求 | 只读取传入的 `points` 等属性，不写业务逻辑 |

新增例外时必须同时更新本表，并在代码注释中写明"为何不能用 objectName + C++ 连接实现"。

## 扩展点与注意事项

- **新增页面**：在 `qml/pages/` 新增文件 → 加入 `CMakeLists.txt` 的 `QML_FILES` → 在 `MainWindow.qml` 的页面栈中追加 → 在 `AppBridge::Page` 与 `UiConnector` 中登记。
- **新增组件**：放入 `qml/components/`（通用）或对应页面目录（专属）；样式一律引用令牌。
- **命名约定**：QML 内部 id / 属性用 `camelCase`；`objectName` 面向 C++ 连接，必须与 `UiConnector` 中的字符串一致。
- **迁移状态**：界面正从 Qt Widgets（`DGLABClient` + `.ui` + `qcss`）逐页迁移到 QML，迁移完成后旧代码与样式表将整体移除。
