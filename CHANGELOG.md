# Changelog

本文档记录 DG-LAB-Client 所有 notable 的版本变更。

版本号格式遵循 [语义化版本 2.0.0](https://semver.org/lang/zh-CN/)  
> **注意**：当前主版本号为 0（表示开发测试阶段，尚不具备获取数据功能，只有处理数据、发送指令能力）。

---

## [Unreleased] (暂无版本)

### Added
- 无

### Changed
- 无

### Deprecated
- 无

### Removed
- 无

### Fixed
- 无

### Security
- 无

---

## [v0.4.0] - 2026-04-17

### Added
- 新增 12 种主题，详细见 [qcss/README.md](qcss/README.md)。
- 样式系统再次重构：使用 `type` 和 `theme` 属性选择器实现主题切换。
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
- 样式系统全面重构：使用 `type` 和 `mode` 属性选择器实现精细控件分类（导航按钮、操作按钮、标题、标签、输入框等），支持亮色/暗色主题一键切换。
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
- 规则引擎：支持从 JSON 文件加载带 `{}` 占位符的运算规则。
- 规则表格高级编辑：通道/模式列使用下拉框，值模式列提供可视化公式构建器（括号检查、符号插入）。
- 通过启用 Python 子进程通过 TCP 通信支持异步调用（线程池 + 信号槽）。
- GitHub Actions 自动化构建新增不包含 Python 标准运行库与第三方包的精简版本（`-without-Python`），适用于本地已有 Python 环境的用户。
- GitHub Actions 自动将更新日志发布到 GitHub Releases 页面。

### Changed
- 完善配置页面内容，添加配置文件的显示、编辑、保存等功能。
- 日志模块添加支持多个输出接收器（控制台、Qt UI）。
- 添加 Python 模块日志输出支持，日志等级与主程序同步。
- 控制台显示格式优化。
- 从 `build.yml` 移除上传 构建产物到 GitHub Releases 的步骤，改为单独的 `release.yml` 处理发布流程。

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
- 用于与 DG-LAB 官方提供的 websocket 服务通讯的 Python 模块：`WebSocketCore.py`。
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

**变动**：[v0.3.0]: https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.2.1...v0.3.0

**变更分类**：
  - `Added` – 新增功能
  - `Changed` – 现有功能变更
  - `Deprecated` – 标记即将移除的功能
  - `Removed` – 移除功能
  - `Fixed` – Bug 修复
  - `Security` – 安全相关修复
