## 源代码目录（src）

本目录包含项目的所有 C++ 源文件（`.cpp`），每个文件对应一个模块的实现。  
项目主要功能包括：基于 Qt 的 GUI 客户端、Python 脚本执行器（支持普通单线程与线程池两种模式）、多级配置管理、模块化日志输出等。

---

## 文件说明

| 文件名 | 描述 |
|--------|------|
| `AppConfig.cpp` | 应用配置主类（`AppConfig`）的实现，采用单例模式。负责配置系统的初始化、销毁、配置项的读写（支持点分隔路径）、配置监听器管理、配置文件的导入导出，并集成了 `MultiConfigManager` 实现多级配置（main/user/system）的优先级合并与热重载。 |
| `ConfigManager.cpp` | 单个配置管理器（`ConfigManager`）的实现，封装了 JSON 配置文件的加载、保存、键值访问（支持默认值）、批量更新（`merge_patch`）、删除指定键路径及变更通知（观察者模式）。内部使用递归互斥锁保证线程安全，并缓存键路径分割结果以提升性能。 |
| `ConfigStructs.cpp` | 配置结构体的定义与实现，目前仅包含 `MainConfig` 结构体，提供与 JSON 的相互转换（`to_json`/`from_json`）及基本的字段有效性验证（`validate`）方法。 |
| `Console.cpp` | Windows 控制台辅助类（`Console`）的实现，采用单例模式。用于在 GUI 程序启动时分配或附加调试控制台，设置 UTF‑8 代码页、字体，并重定向 `stdout`/`stderr`/`stdin`，方便输出调试信息。 |
| `DebugLog.cpp` | 日志系统核心实现（`DebugLog`），单例模式。支持模块级日志等级过滤、多个输出接收器（sink，如控制台、Qt UI）、线程安全的日志写入。提供便捷的宏 `LOG_MODULE` 用于统一格式的日志输出。 |
| `DefaultConfigs.cpp` | 默认配置提供类（`DefaultConfigs`），静态方法 `get_default_config` 根据配置名称（如 "main"、"system"、"user"）返回对应的默认 JSON 配置，用于配置文件的初始化。 |
| `DGLABClient.cpp` | Qt 主窗口类（`DGLABClient`）的实现，继承自 `QWidget`。负责界面初始化（加载样式表、图片、设置属性）、按钮事件绑定、日志显示控件（支持按日志等级着色）、以及异步调用 Python 执行器进行 WebSocket 连接与断开操作（通过 `QtConcurrent` 和信号槽机制）。 |
| `MultiConfigManager.cpp` | 多配置管理器（`MultiConfigManager`）的实现，维护多个 `ConfigManager` 实例的注册表。支持按优先级（`__priority` 字段）排序配置，合并读取时优先级高的配置覆盖优先级低的配置；提供文件热重载功能（自动检测文件修改时间并重新加载）。 |
| `PyExecutor.cpp` | Python 执行器基类（`PyExecutor`）的实现，基于 pybind11 封装。提供 Python 解释器的初始化、模块导入、类实例创建、方法列表获取、同步方法调用（`call` 模板方法）、以及 `eval`/`exec` 执行 Python 代码。所有 Python 操作均通过 GIL 管理。 |
| `PyExecutorManager.cpp` | Python 执行器管理器（`PyExecutorManager`）的实现，单例模式。负责注册、注销不同类型的执行器（普通 `PyExecutor` 或线程池版 `PyThreadPoolExecutor`），提供统一的接口查询已注册的执行器、获取方法列表，并支持对执行器实例的访问（用于同步调用）。注销线程池执行器时会等待正在执行的任务完成（超时机制）。 |
| `PyThreadPoolExecutor.cpp` | 线程池版 Python 执行器的实现。内部定义 `Impl` 结构，包含工作线程池、任务队列、条件变量及 `PyExecutor` 实例。构造函数启动指定数量的工作线程（默认使用硬件并发数），每个工作线程从队列中取出 `Task`（包含 `std::function<void()>`）并执行。提供 `wait_all` 和 `wait_all_for` 方法等待所有任务完成，以及查询活跃/待处理任务数量的接口。 |
