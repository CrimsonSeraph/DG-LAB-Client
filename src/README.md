# 源代码目录 (src)

本目录包含项目的所有 C++ 源文件（`.cpp`），每个文件对应一个模块的实现。  
项目主要功能包括：基于 Qt 的 GUI 客户端、Python 子进程管理（通过 QProcess + TCP Socket）、多级配置管理、模块化日志输出、灵活的规则引擎（支持占位符模式替换）等。

---

## 文件说明

| 文件名 | 描述 |
| - | - |
| `AppConfig.cpp` | 应用配置主类（`AppConfig`）的实现，采用单例模式。负责配置系统的初始化、销毁、配置项的读写（支持点分隔路径）、配置监听器管理、配置文件的导入导出，并集成了 `MultiConfigManager` 实现多级配置（main/user/system）的优先级合并与热重载。 |
| `ComboBoxDelegate.cpp` | `ComboBoxDelegate` 的实现，为规则表格的“通道”和“模式”列提供下拉选择编辑器 |
| `ConfigManager.cpp` | 单个配置管理器（`ConfigManager`）的实现，封装了 JSON 配置文件的加载、保存、键值访问（支持默认值）、批量更新（`merge_patch`）、删除指定键路径及变更通知（观察者模式）。内部使用递归互斥锁保证线程安全，并缓存键路径分割结果以提升性能。 |
| `ConfigStructs.cpp` | 配置结构体的定义与实现，包含 `MainConfig`、`SystemConfig`、`UserConfig` 三个结构体。每个结构体均提供与 JSON 的相互转换（`to_json`/`from_json`）及基本的字段有效性验证（`validate`）方法，用于类型安全的配置访问。 |
| `Console.cpp` | Windows 控制台辅助类（`Console`）的实现，采用单例模式。用于在 GUI 程序启动时分配或附加调试控制台，设置 UTF‑8 代码页、字体，并重定向 `stdout`/`stderr`/`stdin`，方便输出调试信息。 |
| `DebugLog.cpp` | 日志系统核心实现（`DebugLog`），单例模式。支持模块级日志等级过滤、多个输出接收器（sink，如控制台、Qt UI）、线程安全的日志写入。提供便捷的宏 `LOG_MODULE` 用于统一格式的日志输出。 |
| `DefaultConfigs.cpp` | 默认配置提供类（`DefaultConfigs`），静态方法 `get_default_config` 根据配置名称（"main"、"system"、"user"）返回对应的默认 JSON 配置，用于配置文件的初始化。 |
| `DGLABClient.cpp` | Qt 主窗口类（`DGLABClient`）的实现，继承自 `QWidget`。负责界面初始化（加载样式表、图片、设置属性）、按钮事件绑定、日志显示控件（支持按日志等级着色）、规则管理 UI（规则文件选择、表格展示、添加/编辑/删除规则），以及通过 `PythonSubprocessManager` 异步调用 Python 子进程进行 WebSocket 连接与断开操作（基于 `QThreadPool` 和信号槽机制）。 |
| `FormulaBuilderDialog.cpp` | `FormulaBuilderDialog` 的实现，提供可视化公式构建器（支持 `{}`、`+-*/()`、括号检查） |
| `MultiConfigManager.cpp` | 多配置管理器（`MultiConfigManager`）的实现，维护多个 `ConfigManager` 实例的注册表。支持按优先级（`__priority` 字段）排序配置，合并读取时优先级高的配置覆盖优先级低的配置；提供文件热重载功能（自动检测文件修改时间并重新加载）。 |
| `PythonSubprocessManager.cpp` | Python 子进程管理器的实现。基于 `QProcess` 启动外部 Python 脚本，通过解析脚本输出的端口号建立 TCP 连接（`QTcpSocket`），实现 C++ 与 Python 的 JSON 通信。提供异步调用接口（`call`），支持超时和回调，内部使用线程池（`QThreadPool`）避免阻塞主线程。 |
| `Rule.cpp` | 规则类（`Rule`）的实现。单个规则包含名称和带占位符 `{}` 的模式字符串，支持解析占位符位置、统计占位符数量，并提供 `evaluate` 方法将整数数组填充到模式中生成最终字符串。 |
| `RuleManager.cpp` | 规则管理器（`RuleManager`）的实现，单例模式。负责扫描指定目录下的 JSON 规则文件（含特定关键字 `rule`），加载/保存规则文件，管理当前规则集，提供规则的增删改查以及显示字符串生成等接口。规则文件中的 `rules` 对象被解析为 `Rule` 对象集合。 |
| `ValueModeDelegate.cpp` | `ValueModeDelegate` 的实现，负责“值模式”列的双击弹出公式构建对话框 |

---

## 编译依赖

- **C++20** 或更高版本
- **Qt 5/6**（用于 GUI 部分，需包含 Core、Widgets、Concurrent、Network 模块）
- **nlohmann/json**（JSON 解析，仅包含头文件，推荐 v3.11+）

> 注：`json.hpp` 为第三方库，不在此目录中，使用时需自行添加包含路径。Qt 需正确配置包含目录和库链接。

---

## 设计要点

- **配置系统**：采用多级配置（main、user、system）与优先级合并，支持运行时动态修改与热重载，所有配置变更通过监听器通知上层模块。
- **日志系统**：支持模块粒度的日志等级控制，可注册多个接收器（如控制台、UI 控件），便于调试与问题追踪。
- **Python 集成**：通过 `QProcess` 启动独立的 Python 子进程，子进程输出监听的 TCP 端口号，主进程通过 `QTcpSocket` 与之建立连接，使用 JSON 格式进行双向通信。所有耗时调用均提交到全局线程池（`QThreadPool`）中执行，完成后通过信号槽将结果传回主线程，确保 GUI 界面流畅。
- **规则引擎**：支持从 JSON 文件中加载带占位符的模式规则，可动态填充参数生成输出字符串。规则文件按关键字过滤，支持多文件管理（创建、删除、切换），适用于需要灵活配置行为的场景（如命令生成）。
- **线程安全**：所有共享数据结构均使用互斥锁（`std::mutex`、`std::shared_mutex`、`std::recursive_mutex`）或原子变量保护，确保多线程环境下的正确性。
- **GUI 响应**：`DGLABClient` 将 Python 调用放入后台线程执行，通过信号与槽机制将结果传回主线程更新界面，避免阻塞 UI。规则管理操作同步执行，不影响整体流畅度。
