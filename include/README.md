# 头文件目录 (include)

本目录包含项目的所有头文件（`.h` 和 `.hpp`），声明了各个模块的类、结构体及模板实现。  
其中 `json.hpp` 为第三方库，仅用于 JSON 序列化/反序列化，非本项目代码。

---

## 文件说明

| 文件名 | 描述 |
|--------|------|
| `AppConfig.h` | 应用配置主类声明，提供单例访问、配置项读写、监听器注册等接口。 |
| `AppConfig_impl.hpp` | 包含 `AppConfig` 的模板方法实现，以及配置值包装器 `ConfigValue` 和 `ConfigObject`。 |
| `ConfigManager.h` | 单个配置管理器类声明，提供 JSON 配置的加载、保存、键路径访问及变更通知。 |
| `ConfigStructs.h` | 配置结构体声明，如 `MainConfig`，并定义 JSON 序列化与验证接口。 |
| `Console.h` | Windows 控制台辅助类声明，用于创建/销毁调试控制台。 |
| `DebugLog.h` | 日志系统接口声明，包含日志等级枚举及宏 `LOG_MODULE`。 |
| `DGLABClient.h` | Qt 主窗口类声明，继承自 `QWidget`，声明 UI 控件及槽函数。 |
| `json.hpp` | 第三方库 [nlohmann/json](https://github.com/nlohmann/json) 的单头文件版本，用于处理 JSON 数据。 |
| `MultiConfigManager.h` | 多配置管理器类声明，管理多个配置，支持优先级合并与热重载。 |
| `MultiConfigManager_impl.hpp` | 包含 `MultiConfigManager` 的模板方法实现，如按优先级或名称获取/设置配置值。 |
| `PyExecutor.h` | Python 执行器基类声明，封装 pybind11 的基本操作（模块导入、方法调用等）。 |
| `PyExecutor_impl.hpp` | 包含 `PyExecutor` 的模板方法实现，如 `call_sync`、`call_async` 等。 |
| `PyExecutorManager.h` | Python 执行器管理器类声明，用于注册/注销执行器并提供统一调用接口。 |
| `PyExecutorManager_impl.hpp` | 包含 `PyExecutorManager` 的模板方法实现，如 `call_sync` 和 `call_async`。 |
| `PyThreadPoolExecutor.h` | 线程池版 Python 执行器类声明，继承自 `PyExecutor` 并添加任务队列和工作线程。 |
| `PyThreadPoolExecutor_impl.hpp` | 包含 `PyThreadPoolExecutor` 的模板方法实现，如 `submit` 和异步调用。 |

---

## 使用说明

- 所有头文件均采用 `#pragma once` 防止重复包含。
- 模板实现分离至对应的 `_impl.hpp` 文件中，需在使用时包含相应头文件（通常在主头文件末尾已包含）。
- `json.hpp` 需提前下载并放置于包含路径中，或通过包管理器（如 vcpkg、conan）引入。
- 本项目的日志系统通过 `DebugLog.h` 中的宏 `LOG_MODULE` 输出，可在编译时开启/关闭。
