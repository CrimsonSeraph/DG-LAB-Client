# Changelog

本文档记录 DG-LAB-Client 所有 notable 的版本变更。

版本号格式遵循 [语义化版本 2.0.0](https://semver.org/lang/zh-CN/)  
> **注意**：当前主版本号为 0（表示开发测试阶段，尚不具备获取数据功能，只有处理数据、发送指令能力）。

---

## [Unreleased]

### Added
- 暂无。

### Changed
- 暂无。

### Deprecated
- 暂无。

### Removed
- 暂无。

### Fixed
- 暂无。

### Security
- 暂无。

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

**变动**：[v0.2.0]: https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.1.0...v0.2.0

**变更分类**：
  - `Added` – 新增功能
  - `Changed` – 现有功能变更
  - `Deprecated` – 标记即将移除的功能
  - `Removed` – 移除功能
  - `Fixed` – Bug 修复
  - `Security` – 安全相关修复
