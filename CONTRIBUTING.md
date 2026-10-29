# Contributing to DG-LAB-Client

感谢你考虑为 DG-LAB-Client 贡献代码！我们欢迎任何形式的贡献，包括提交 Bug 报告、功能请求、代码改进、文档更新等。

参与贡献前请先阅读本指南，以便你的贡献能顺利合并。

## 行为准则

本项目遵守 [贡献者公约](https://www.contributor-covenant.org/version/2/1/code_of_conduct/)，具体细则请参阅项目根目录下的 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。

参与即表示你同意遵守其条款。

## 如何报告问题（Issue）

如果你发现了 Bug、有功能建议或对文档有疑问，请在 [GitHub Issues](https://github.com/CrimsonSeraph/DG-LAB-Client/issues) 页面提交。

### 提交前请先检查

- 搜索现有 Issues，避免重复提交。
- 确认你使用的是最新版本（或相关的 commit）。

### Bug 报告模板

请尽量按以下模板填写：

```markdown
**描述** 清晰简洁地描述问题是什么。

**复现步骤**

1. 执行 ... 命令/操作
2. 点击 ... 按钮
3. 看到错误信息 ...

**预期行为** 你期望发生什么？

**截图/日志** 如果有，请附上相关截图或控制台输出日志（注意脱敏）。

**环境信息**

- 操作系统: Windows 10 / macOS 14 / Ubuntu 22.04
- Qt 版本: 5.15.2 / 6.5.0
- Python 版本: 3.9 / 3.11
- 项目版本或 commit hash: v0.5.1 或 xxxxxx

**额外信息** 其他你觉得有用的信息。
```

### 功能请求模板

```markdown
**你的功能请求与什么问题相关？** 清晰描述背景和痛点。

**描述你想要的解决方案** 简明扼要。

**描述你考虑过的替代方案** 如果有。

**其他** 补充说明。
```

## 如何贡献代码（Pull Request）

我们欢迎 Pull Request（PR）。提交前请确认你的修改符合以下要求。

### 1. 分支与同步

- 从 `main` 分支创建你的功能分支：`git checkout -b feature/your-feature-name` 或 `fix/bug-description`。
- 提交前请先 rebase 到最新的 `main` 分支：`git fetch origin && git rebase origin/main`。
- 解决可能出现的冲突。

### 2. 代码风格

编码规范见 [CodingStyle.md](CodingStyle.md)，请务必遵守。

- 编码：UTF‑8 without BOM，换行符 CRLF（Windows 风格），文件末尾保留一个空行。
- 缩进：4 个空格，不使用 Tab。
- 命名：
    - C++ 类：`PascalCase`
    - 变量/函数：`snake_case`
    - 常量/宏：`UPPER_CASE`
    - Python 脚本文件名：`PascalCase`（如 `PathFinder.py`）
    - Qt 虚函数（如 `paintEvent`）保持 Qt 原有风格。
- 头文件：使用 `#pragma once`。
- 头文件分区顺序：`public:` → `protected:` → `private:` → `signals:` → `private slots:`。每个分区内部按规则排列。
- 源文件：函数定义顺序必须与头文件声明顺序一致。
- 注释：复杂函数使用 Doxygen 风格（`/// @brief` 等）；简单函数可用单行注释。
- 日志：合理使用 `LOG_MODULE` 宏，避免高频输出。
- 导入顺序：自定义模块 > 第三方库 > 标准库，每组内按字母顺序排列。
- 注释细则：独佔一行的注释 `//` 后跟空格并与代码同缩进；行尾注释与代码间隔一个 Tab。注释只说明功能，不解释原因。

> 如果你的编辑器支持，可以配置自动格式化（项目根目录有 `.editorconfig`）。

### 3. Commit 规范

推荐使用语义化的提交信息格式：

```
<type>(<scope>): <subject>

<body>

<footer>
```

- type：`feat`（新功能）、`fix`（Bug 修复）、`docs`（文档）、`style`（格式，不影响代码逻辑）、`refactor`（重构）、`perf`（性能优化）、`test`（测试）、`chore`（构建/工具变动）
- scope（可选）：影响的模块，如 `config`、`rule`、`python`、`ui` 等
- subject：简短描述，不超过 50 字符，使用英文或中文（尽量英文）
- body（可选）：详细描述改动的原因和内容
- footer（可选）：关闭 Issue 用 `Closes #123`

示例：

```
feat(rule): add formula builder dialog

Add a dialog for editing value pattern with syntax checking.
Closes #42
```

```
fix(python): handle missing Steam library gracefully

Prevent PathFinder.py from aborting when the Steam library file is absent.
```

### 4. 测试要求

- 确保你的修改可以通过编译（至少使用 MSVC 和 GCC 之一）。
- 如果添加了新功能，请尽量在本地手动测试主要场景。
- 对于影响 GSI 插件的修改，请测试 `PathFinder.py` 能找到游戏目录，且插件能生成 GSI 配置并接收数据。
- 目前项目未配置自动化单元测试，CI 会执行编译检查；欢迎补充测试用例。

### 5. 提交 PR

- 推送到你自己的 fork 仓库后，在 GitHub 上发起 Pull Request。
- PR 标题应简明扼要，内容描述：
    - 解决了什么问题（关联 Issue 编号）。
    - 改动的简要概述。
    - 如果引入破坏性更改，请说明。
- 确保 PR 的 base 分支是 `main`。
- 如果 CI 检查（GitHub Actions）失败，请及时修复。
- 等待代码审查。审查者可能会要求修改，请积极回应。

### 6. 关于依赖更新

- 如果你需要升级 Qt 版本或 Python 依赖，请先在 Issue 中讨论，因为可能影响构建环境。
- 修改 `CMakeLists.txt` 中的依赖下载方式时，注意保持向后兼容。

## 文档贡献

文档（`README.md`、`CodingStyle.md`、`CONTRIBUTING.md` 等）的改进同样欢迎。请遵循与代码相同的 PR 流程，并在 `docs` 类型的 commit 中注明。

## 项目不可变约定

以下约定不接受逐案讨论，改代码与改文档前先对齐。

### 文档

- 每个目录的 README.md 与 `docs/` 下文档必须与实现同步：改接口、字段、行为要同步更新对应文档。
- 文档简洁凝练，直接说明功能与用法；**严禁用 `===` 或 `---` 作分割线**。
- 修改大文档时用单子串编辑，**禁止整文件读入再写回**（历史上因此丢失过 71 行）。
- 新建文档：UTF-8 无 BOM、CRLF、文件末尾一个空行；相对链接必须能解析到真实文件。

### 提交

- 每个小任务完成后提交，格式：

```
<type>(<scope>): <subject>
- <body>
- <body>
```

  type 与 scope 用英文，subject 与 body 用中文，body 简述关键变动。
- 提交前要求构建通过（`cmake --build` 0 error）；单元测试默认不参与构建，需要时用 `-DDGLAB_BUILD_TESTS=ON` 配置后跑到 `ctest` 全绿。
- 提交与推送由任务编排者统一执行；自动化代理不自行运行 git 写操作。
- 提交信息只需在最后一次提交 / 发布时确保符号规范，日常开发不必严格苛求。

### UI 视觉

- 颜色只用 `Theme` 令牌，**禁止任何颜色字面量**（hex / `Qt.rgba` / 命名色）；当前默认主题是 `classic`（近黑底 + 淡黄强调）。
- 参考截图在设计稿目录中，与文字需求冲突时**以文字需求为准**。

## 获取帮助

如果你不确定如何开始，可以在 Issue 中提问，或通过作者联系方式（见 README）询问。

再次感谢你的贡献！
