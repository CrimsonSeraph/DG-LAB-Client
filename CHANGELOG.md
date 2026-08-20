# Changelog

本文档记录 DG-LAB-Client 所有 notable 的版本变更。

版本号格式遵循 [语义化版本 2.0.0](https://semver.org/lang/zh-CN/)  
> **注意**: 当前主版本号为 0（表示开发测试阶段，尚不具备获取数据功能，只有处理数据、发送指令能力）。

---

## [Unreleased]

### Added
- **数值模块（Module）**: 新增 `ModuleValue`/`Module`/`ModuleManager` 数据模型（`include/Module.h`、`include/ModuleValue.h`、`include/ModuleManager.h` 及对应源文件），默认注册 CS2 GSI 模块，内置 `health`（m_iHealth）、`armor`（m_ArmorValue）、`team_num`、`money`、`has_helmet`、`has_defuser` 等数值（参照 CS2 官方 GSI 规范）。
- **模块页面**: 点击模块卡片弹出数值展示窗口（`ModuleValuesDialog`，每行两个数值框：名称 + 当前值 + 底层字段名小字），每个数值可独立设置查询周期（每秒/每两秒/每四秒/每半秒/四分之一秒），模块页面提供统一设置入口。
- **周期调度机制**: 以所有数值中最短查询周期为基准的调度算法（最短 250ms 精确计时），周期为基准整数倍的数值按对应倍率间隔查询；周期设置变化时自动重建调度器；数值变化时通过 `value_changed` 信号推送，未变化不推送。
- **规则数据结构扩展**: 规则新增 `enabled` 启用状态、多父级（通道 A/B 与规则序号，可混合）、唯一规则序号（按加载顺序编号）；启用逻辑为父级非空且任一父级可用（通道启用/父级规则启用），不影响其他分支。
- **值模式扩展**: 支持 `{id:xxx(名称)}` 模块数值引用与 `{rule:xx}` 规则结果引用占位符；空值语义——任一引用为空时本次计算被忽略（不推送、不发送）。
- **计算表达式编辑器**: 新增“显示可用数值”按钮，弹出菜单列出模块数值（名称 + ID）与除自身外的所有规则（含父级为通道的规则，按规则序号插入 `{rule:xx}`）；`{id:xxx(名称)}` 括号注释部分以灰色显示（`PlaceholderHighlighter` 高亮）。
- **规则间引用与级联触发**: `{rule:xx}` 引用其他规则计算结果（优先缓存，未计算则递归计算，深度保护防循环）；规则计算完成后结果推送给所有父级（规则父级同样触发计算，通道父级通过 `rule_command_ready` 信号发送给 Python 端）；模块数值变化时自动触发引用该数值的规则；A/B 通道启用状态联动规则引擎。
- **规则页面**: 表格新增“启用”列（勾选框，点击切换启用状态）；新增“编辑父级”按钮与 `ParentEditDialog`（通道父级单选 + 规则父级多选，通过增删目标规则值模式中的 `{rule:xx}` 实现）；“通道”列更名为“父级”列，显示通道/规则引用组合（如 `A;rule:1,2`），模式列按父级类型灰显（`(不适用)`/`(部分不适用)`，黑白主题颜色互反）；通道父级唯一性去重（加载时保留序号最小，手动设置保留最后设置）。
- **首页通道面板**: 改造 `x_normal_cards`——模块区域显示挂载在该通道上的模块名称与模块内数值的最小查询周期，规则区域显示父级为该通道的规则名称与最近一次计算的数值（规则计算完成时实时刷新）；`x_wave_card` 保留现状。
- **勾选框样式**: 规则表格启用列勾选框增加 `QTableWidget::indicator` 样式（未选中空心、选中强调色填充 + 勾号），新增 `check_white.svg`/`check_dark.svg` 资源，14 个主题统一应用。

### Changed
- Windows 构建: Python 标准库 zip 打包优化——排除 site-packages（约 5GB 第三方包）、__pycache__/*.pyc 与 test，改用系统内置 bsdtar 打包，configure 耗时由数十分钟降至数秒，zip 体积约 1GB 降至约 5MB，且 zipimport 可直接导入。
- 统一全项目注释规范: 头文件函数补齐 Doxygen 注释（中文 @brief/@param/@return），源文件行尾注释全部改为独立行注释。
- **规则文件格式**: 新增 `enabled`（bool）与 `parents`（数组，`"A"`/`"B"` 字符串或规则序号整数）字段，`valuePattern` 支持 `{id:xxx(名称)}`/`{rule:xx}` 占位符；兼容旧 `channel` 字段（未提供 `parents` 时作为唯一父级）。
- **规则保存**: 按规则序号排序输出，保证序号稳定；`FormulaBuilderDialog` 改用 `QTextEdit` 编辑并支持灰色注释显示。
- **首页布局**: 放宽 `x_normal_cards` 高度限制，通道卡片自适应布局。

### Deprecated
- 无

### Removed
- 无

### Fixed
- 修复新版 macOS SDK 移除 AGL.framework 导致的链接失败（ld: framework 'AGL' not found）：从 Qt 导入目标中剥离 AGL 引用，并显式链接 OpenGL.framework。
- 构建系统: nlohmann/json.hpp 下载失败留下的 0 字节空文件现在会被识别并重新下载，避免误判为已存在而跳过下载。
- 修复关闭窗口时托盘图标为空导致的野指针崩溃隐患（tray_icon_ 判空）。
- 修复 ValueModeDelegate 事件转发传递默认构造 option 导致未处理事件状态丢失的问题。
- 修复创建规则文件时规则文件列表被重复刷新的问题。
- 修复 ConfigManager::validate 端口校验键名错误（app.server_port -> app.websocket.port）。
- 清理 MultiConfigManager 重复 include 分支、IpSelector 未使用变量、AppConfig 未使用锁变量及 DebugLog 重复查找等冗余代码。
- 修复计算表达式“显示可用数值”规则列表误过滤父级为通道的规则，现包含除自身外的所有规则（引用通道规则不要求对应通道已启用）。
- 修复规则表格启用列勾选框因样式覆盖难以分辨选中状态的问题。

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

**变动**:[v0.6.0]: https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.5.1...v0.6.0

**变更分类**: 
  - `Added` – 新增功能
  - `Changed` – 现有功能变更
  - `Deprecated` – 标记即将移除的功能
  - `Removed` – 移除功能
  - `Fixed` – Bug 修复
  - `Security` – 安全相关修复
