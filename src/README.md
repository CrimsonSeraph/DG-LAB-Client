# 源代码目录 (src)

本目录包含项目的所有 C++ 源文件（`.cpp`），每个文件对应一个模块的实现。  
项目主要功能包括：基于 Qt 的 GUI 客户端、Python 脚本执行器、多级配置管理、日志输出等。

---

## 文件说明

| 文件名 | 描述 |
|--------|------|
| `AppConfig.cpp` | 应用配置主类的实现，负责初始化、销毁、读写配置项，管理配置监听器及配置文件的导入导出。 |
| `ConfigManager.cpp` | 单个配置管理器的实现，封装 JSON 文件的加载、保存、键值访问及变更通知。 |
| `ConfigStructs.cpp` | 配置结构体的实现（目前仅包含 `MainConfig` 的序列化与验证方法）。 |
| `Console.cpp` | Windows 控制台辅助类的实现，用于创建/销毁调试控制台并重定向标准流。 |
| `DebugLog.cpp` | 日志系统核心实现，支持模块级日志等级过滤及线程安全的输出。 |
| `DGLABClient.cpp` | Qt 主窗口类的实现，处理界面初始化、样式加载及按钮点击事件。 |
| `MultiConfigManager.cpp` | 多配置管理器实现，维护多个 `ConfigManager` 实例，支持按优先级合并读取、热重载。 |
| `PyExecutor.cpp` | Python 执行器基类的实现，封装 pybind11，提供模块导入、方法调用（同步/异步）及实例创建功能。 |
| `PyExecutorManager.cpp` | Python 执行器管理类的实现，负责注册/注销执行器（普通或线程池版），并提供统一的调用接口。 |
| `PyThreadPoolExecutor.cpp` | 线程池版 Python 执行器的实现，基于 `PyExecutor` 添加任务队列和工作线程。 |

---

## 编译依赖

- C++20 或更高版本
- Qt 5/6（用于 GUI 部分）
- pybind11（用于 Python 集成）
- nlohmann/json（JSON 解析，仅包含头文件）

> 注：`json.hpp` 为第三方库，不在此目录中，使用时需自行添加包含路径。
