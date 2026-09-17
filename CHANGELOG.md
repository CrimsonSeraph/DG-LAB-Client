# Changelog

本文档记录 DG-LAB-Client 所有 notable 的版本变更。

版本号格式遵循 [语义化版本 2.0.0](https://semver.org/lang/zh-CN/)

> **注意**: 当前版本为 v1.0.0，已具备数据获取与处理能力（数值模块、插件化数据接入如 CS2 GSI、规则引擎等）。

---

## [Unreleased]

### Added

- **QML 界面层（迁移中）**: 新增 `Dglab` QML 模块（`MainWindow` / 页面 / 组件 / `style` 单例），以 `qt_add_qml_module` 接入构建；界面采用左侧导航 + 页面栈 + 状态栏结构（原生标题栏）。
- **主题令牌系统**: 新增 `ThemeManager`（注册为 QML 单例 `Theme`），提供 14 套预设主题与自定义主/副色的语义化颜色令牌，自定义色持久化到 `user.json`（`app.ui.custom.primary` / `app.ui.custom.secondary`）。
- **界面桥接对象**: 新增 `AppBridge`（应用信息与页面导航）、`DeviceController`（连接状态、地址读写、二维码、强度/波形/清除指令与设备回传解析）、`HomeBridge`（A/B 通道强度、上限、启用状态、模块摘要与规则摘要）。
- **首页 QML 通道面板**: A/B 通道面板包含强度（可编辑目标值、± 快捷、上限显示）、模块摘要、规则摘要（最近计算值）、波形入口与启停按钮。
- **配置页连接与主题卡片**: 连接卡片（IP / 端口 / 连接断开 / 二维码）；主题卡片合并为单卡，展示当前主题名称、主色与副色，支持预设主题网格与 `ColorDialog` 自定义取色。
- **集中式 QML↔C++ 连接**: `UiConnector` 按 `objectName` 统一建立交互连接；列表/中继器动态委托通过可视子树扫描连接。
- **波形库与波形编辑器**: 新增 `Wave`/`WaveLibrary`（`config/waves/waves.json`，内置三种波形）与 `WaveBridge`；QML 提供 `WavePreview`（Shape 强度/频率双曲线）、`WaveSelectDialog`（"确定波形"改为"选择波形"）与 `WaveEditorDialog`（段落+关键帧、原始 V3 帧、实时预览、保存/发送）。
- **应用内 WebSocket 中转服务**: 新增 `DglabRelayServer`，由应用直接托管 V3（9999）与 V4（9998）中转服务（配对、心跳、强度/波形/清除、V4 设备与 `device.op`），不再依赖 Node 官方后端与 Python `Bridge.py`/`WebSocketCore.py`。
- **内置二维码**: 内嵌 Nayuki `qrcodegen`（MIT，`third_party/qrcodegen`）与 `QrImageProvider`，配对二维码由应用自身生成。
- **规则可视化编辑器**: 新增 `RuleGraph`（规则/模块源/运算符/高级/通道输出节点与连线模型、规则 JSON 双向转换、侧车文件保存位置）与 `RuleGraphBridge`；QML 提供节点画布（平移/缩放、拖拽移动、拖拽连线、右键建节点、Ctrl+C/V、Delete）、模块数值面板与节点检查器；首页与配置页入口打开编辑器。
- **蓝牙直连（郊狼 V3）**: 新增 `CoyoteBleController`，按官方 V3 蓝牙协议扫描设备（47L121000 / 47L120100）、连接、每 100ms 写 B0（强度 + 双通道波形）、写 BF 软上限与平衡参数、解析 B1 强度回传与电量；配置页新增蓝牙卡片（扫描/连接/设备列表/电量/强度增减）。

### Changed

- 程序入口由 `QApplication` + `DGLABClient` 改为 `QGuiApplication` + `QQmlApplicationEngine`；界面统一使用 `Basic` 控件样式，外观由样式令牌控制。
- 数值模块与规则引擎改为在启动时初始化并加载默认规则文件。
- 依赖调整：新增 Qt WebSockets / Bluetooth，移除 Qt Widgets；新增内嵌 `qrcodegen`；不再需要 Python `websockets` / `qrcode`。
- CMake 在 Windows 构建后自动调用 `windeployqt` 把 Qt 运行时部署到输出目录（原先仅在打包阶段执行）。

### Deprecated

- 无

### Removed

- **Qt Widgets 旧界面**: 移除 `DGLABClient`（含 `.ui`、`_impl`、`_utils`）与配套控件 `ThemeSelectorDialog`、`EditableLabel`、`StyledComboBox`、`SampledWaveformWidget`、`IpSelector`。
- **旧规则编辑 UI**: 移除 `FormulaBuilderDialog`、`ParentEditDialog`、`ComboBoxDelegate`、`ValueModeDelegate`（由可视化规则图取代）。
- **旧对话框**: 移除 `ModuleValuesDialog`、`LogExportSettingsDialog`（日志导出位置/保留数量/大小上限改为直接编辑 `user.json`）。
- **Python WebSocket 通信层**: 移除 `PythonSubprocessManager`、`python/Bridge.py`、`python/WebSocketCore.py`（保留 `python/PathFinder.py` 供 GSI 插件使用）。
- **qcss 主题样式表**: 移除 `qcss/` 目录及其资源、拷贝与安装规则（主题改由 `ThemeManager` 提供）。

### Fixed

- 无

### Security

- 无

---

## [v1.0.0] - 2026-08-26

### Added

- **数值模块系统**: 新增 `ModuleValue`/`Module`/`ModuleManager` 数据模型与模块页面，支持周期查询调度（最小 250ms 基准轮询）与数值变化检测推送；数值支持可选最小/最大值（写入自动钳制到范围，弹窗显示当前值/最值）。
- **插件系统**: 新增 `IPlugin` 插件接口、`PLUGIN_EXPORT` 导出宏与 API 版本校验；`ModuleManager` 重构为插件宿主（扫描/加载/卸载/状态管理）；新增示例插件 `module/example` 与插件开发指南。
- **CS2 GSI 数据接入**: 新增 CS2 GSI 模块与 `CS2GsiPlugin` 插件——监听 GSI 端口接收 CS2 游戏数据，注册 23 项数值（个人状态 17 项 + 团队/地图 6 项）并实现自身/队友归属区分；配套新增 `ProcessChecker` 进程检查工具与 `PathFinder.py` 路径查找工具。
- **通用数据接收器 (DataListener)**: 将 `GsiServer` 重构为通用数据接收器，支持 TCP/UDP、来源信封分发与解析器抽象（`IDataParser`）。
- **规则引擎增强**: 规则新增启用状态、多父级（通道 A/B 与规则引用）与唯一序号；值模式支持 `{id:xxx(名称)}`/`{rule:xx}` 占位符与空值语义；实现规则间引用、级联触发与深度保护；规则页面新增启用列、父级编辑对话框与模式列灰显。
- **首页通道面板**: 首页 A/B 通道卡片新增模块信息与规则最近计算结果显示（实时刷新）。
- **日志导出**: 自动日志（分片轮转、数量/大小限制）与手动日志（导出按钮、不受限制）分离，设置持久化到 `user.json`。
- **UI 改进**: 统一下拉框控件 `StyledComboBox`、勾选框样式、表格编辑显示与首页卡片布局修复。

### Changed

- **插件宿主接口**: `IPluginHost` 新增数据接收、配置读写、基础周期与用户通知能力；`IPlugin` 增加周期变化通知。
- **CS2 GSI 插件化**: 原静态 CS2 GSI 模块改造为符合 `IPlugin` 接口的动态库插件（`module/gsi/`）。
- **规则文件格式**: 新增 `enabled` 与 `parents` 字段，兼容旧 `channel` 字段；规则按序号排序保存。
- 源码按功能分类整理（`include/`、`src/` 子目录），统一 Doxygen 中文注释与 Prettier/clang-format 格式规范。
- Windows 构建优化 Python 标准库 zip 打包（configure 耗时由数十分钟降至数秒）。

### Deprecated

- 无

### Removed

- 无

### Fixed

- 修复新版 macOS SDK 缺失 AGL.framework 导致的链接失败。
- 修复托盘图标野指针、下拉框黑色边缘/菜单重叠、表格编辑显示不全等 UI 问题。
- 修复日志清理分组、规则值模式编辑未应用、Python 日志换行与 `WebSocketCore.py` 导入缺失等问题。
- 修复数据源为空时查询返回 0 导致的显示失真、首次写入不推送 `value_changed` 等问题。

### Security

- 无

---

## [v0.6.0] - 2026-05-03

### Added

- **IP 选择器** (`IpSelector`): 单例类，支持基于黑白名单关键词自动匹配可用 IPv4 地址（自动过滤虚拟网卡），并提供图形化对话框让用户编辑黑白名单并手动选择 IP。
- **可编辑标签控件** (`EditableLabel`): 继承自 `QLabel`，支持双击进入编辑模式，内嵌 `QLineEdit` 并支持任意 `QValidator` 验证器，编辑完成发出 `text_edited` 信号。
- **主题更新**: 更新全新 UI 布局与主题，所有主题样式表统一使用 `rgba` 颜色格式，遵循 6:3:1 主副点缀色比例原则。
- 为 `ThemeSelectorDialog` 添加网格卡片式主题预览（显示中文名、英文模式名及主色块），提升用户选择体验。

### Changed

- **样式系统全面翻新**: 所有 QSS 文件转换为纯 `rgba` 颜色值，并按照主色 60%、副色 30%、点缀色 10% 的比例重新调配，使界面色彩更加和谐统一。

### Deprecated

- 计划移除使用 Python 模块 `WebSocketCore.py` 实现 WebSocket 相关功能，转向使用 Qt 提供的 Qt WebSocket 库（该计划尚未完成，仍处于过渡阶段）。

### Removed

- 无

### Fixed

- 修复 `IpSelector` 无法自定义选择 IP 问题。

### Security

- 无

---

## [v0.5.1] - 2026-04-19

### Added

- 波形控件 `SampledWaveformWidget` 支持多监听器（多通道）实时显示。
    - 新增 `add_listener(name, color)`、`remove_listener(name)`、`set_listener_color(name, color)` 等接口。
    - 每个监听器独立采样缓冲区（环形，默认200点）和独立颜色。
    - 提供默认监听器 `"default"`（绿色），兼容旧版单曲线接口。
    - 提供为每个监听器设置输入范围 `set_input_range()` 的两个重载，经过统一化为 0~1 归一化数据输入。
- 增加最大监听器数量限制（默认16，可通过 `set_max_listeners()` 调整）。
- 所有监听器操作及数据输入均添加日志记录，便于调试。
- 为按键添加 `btn_size` 属性 `small` 用于显示小号按键。

### Changed

- 优化 `paintEvent` 绘制性能: 先拷贝监听器快照再绘制，避免长时间持有锁。
- 完善 Doxygen 注释，头文件与源文件分类注释格式规范化。
- 优化 UI 布局。

### Deprecated

- 计划移除使用 Python 模块 `WebSocketCore.py` 实现 websocket 相关功能，转向使用 Qt 提供的 Qt WebSocket 库。

### Removed

- 无

### Fixed

- 无

### Security

- 无

---

## [v0.5.0] - 2026-04-19

### Added

- 新增实时显示强度，提供手动调节、锁定调节按键。

### Changed

- 无

### Deprecated

- 计划移除使用 Python 模块 `WebSocketCore.py` 实现 websocket 相关功能，转向使用 Qt 提供的 Qt WebSocket 库。

### Removed

- 无

### Fixed

- 修复 Python 模块日志格式错误。

### Security

- 无

---

## [v0.4.0] - 2026-04-17

### Added

- 新增 12 种主题，详细见 [qcss/README.md](qcss/README.md)。
- 样式系统再次重构: 使用 `type` 和 `theme` 属性选择器实现主题切换。
- 添加主题选择器 `ThemeSelectorDialog.h/cpp`，支持主题预览与选择。

### Changed

- 为 CMake 构建流程中 `Python 运行时和第三方包安装` 添加检查条件，防止重复打包。
- `user.json` 中关于主题的参数 `app.ui.is_light_mode` 改成 `app.ui.theme`，参数为主题英文名，采用 **全小写+下划线** 形式。

### Deprecated

- 计划移除使用 Python 模块 `WebSocketCore.py` 实现 websocket 相关功能，转向使用 Qt 提供的 Qt WebSocket 库。

### Removed

- 移除原本的主题切换按键。

### Fixed

- 无

### Security

- 无

---

## [v0.3.0] - 2026-04-16

### Added

- 新增实时波形采样控件（`SampledWaveformWidget`），支持连续输入 0~1 归一化数据并以滚动折线图显示，可调节采样间隔和最大振幅比例。
- 样式系统全面重构: 使用 `type` 和 `mode` 属性选择器实现精细控件分类（导航按钮、操作按钮、标题、标签、输入框等），支持亮色/暗色主题一键切换。
- 新增 `apply_widget_properties()` 和 `apply_inline_styles()` 方法，统一为控件设置样式属性和内联样式，提高代码可维护性。
- 添加贡献指南 `CONTRIBUTING.md`。

### Changed

- 完善 `DGLABClient` 的样式管理逻辑，`setup_widget_properties()` 和 `change_theme()` 方法大幅优化，主题切换更流畅。
- 整理所有源文件和头文件的 `#include` 顺序，按自定义 > 第三方 > 标准库分组，提升代码规范性。
- 为多个模块添加更详细的注释，特别是配置系统、规则引擎和 Python 子进程管理部分。
- 减少冗余调试信息输出，仅保留关键状态日志，降低日志噪音。
- 样式表文件 `style_light.qcss` 和 `style_night.qcss` 完全重写，视觉效果现代化。
- 项目协议从 `MIT` 更换为 `GPL-v3.0`，完善第三方的开源信息。

### Deprecated

- 无

### Removed

- 无

### Fixed

- 修复 Linux 系统下 Python 子进程启动时路径解析错误的问题（改用 `QCoreApplication::applicationDirPath()` 拼接绝对路径）。

### Security

- 无

---

## [v0.2.1] - 2026-04-12

### Added

- 无

### Changed

- 创建规则添加反馈信息。
- 减少值模式编辑下过多的调试信息输出。

### Deprecated

- 无

### Removed

- 无

### Fixed

- 统一规则编辑窗口（`添加规则` 和 `编辑规则` 触发的值模式编辑统一使用新编辑窗口）。
- 创建规则文件时检查是否包含关键字，未包含则强制包含关键字。

### Security

- 无

---

## [v0.2.0] - 2026-04-08

### Added

- 规则引擎: 支持从 JSON 文件加载带 `{}` 占位符的运算规则。
- 规则表格高级编辑: 通道/模式列使用下拉框，值模式列提供可视化公式构建器（括号检查、符号插入）。
- 通过启用 Python 子进程通过 TCP 通信支持异步调用（线程池 + 信号槽）。
- GitHub Actions 自动化构建新增不包含 Python 标准运行库与第三方包的精简版本（`-without-Python`），适用于本地已有 Python 环境的用户。
- GitHub Actions 自动将更新日志发布到 GitHub Releases 页面。

### Changed

- 完善配置页面内容，添加配置文件的显示、编辑、保存等功能。
- 日志模块添加支持多个输出接收器（控制台、Qt UI）。
- 添加 Python 模块日志输出支持，日志等级与主程序同步。
- 控制台显示格式优化。
- 从 `build.yml` 移除上传构建产物到 GitHub Releases 的步骤，改为单独的 `release.yml` 处理发布流程。

### Deprecated

- 无

### Removed

- 移除旧版内嵌 Python 解释器。

### Fixed

- 修复配置系统的加载、覆盖等问题。

### Security

- 无

---

## [v0.1.0] - 2026-01-10

### Added

- 初始版本（未发布版本）。
- 基础 Qt 界面（主页、配置页等）。
- 完善的配置系统，支持 JSON 文件读写、监听器通知、默认配置生成等功能。
- 完善的日志系统，支持模块化日志等级控制线程安全写入。
- 清晰的控制台输出格式以及可以限制的日志等级输出。
- 内嵌 Python 解释器执行 Python 模块。
- 用于与 DG-LAB 官方提供的 websocket 服务通讯的 Python 模块: `WebSocketCore.py`。
- GitHub Actions 自动化构建（Windows / Linux / macOS）。

### Changed

- 无

### Deprecated

- 将不再使用内嵌 Python 解释器，转用 Qt 提供的方式调用 Python 子进程并通过 TCP 本地通讯。

### Removed

- 无

### Fixed

- 无

### Security

- 无

---

## 其他

**变动**:

- [v1.0.0]: https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.6.0...v1.0.0
- [v0.6.0]: https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.5.1...v0.6.0

**变更分类**:

- `Added` – 新增功能
- `Changed` – 现有功能变更
- `Deprecated` – 标记即将移除的功能
- `Removed` – 移除功能
- `Fixed` – Bug 修复
- `Security` – 安全相关修复
