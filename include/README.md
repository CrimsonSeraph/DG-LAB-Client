# 头文件目录（include）

本目录包含项目的所有公共头文件（`.h` 及部分模板实现 `.hpp`），定义了应用程序的核心接口、数据结构、配置管理、日志系统、规则引擎以及与 Python 子进程通信的类。  
头文件之间通过前置声明和包含关系相互引用，形成了清晰的模块化结构。

---

## 文件说明

| 文件名 | 描述 |
| - | - |
| `AppConfig.h` | 应用配置主类 `AppConfig`（单例）的声明。提供配置系统的全局入口，负责初始化、销毁、配置项的读写（支持点分隔路径）、监听器管理、批量操作、导入导出等功能。内部集成 `MultiConfigManager` 实现多级配置优先级合并。 |
| `AppConfig_impl.hpp` | `AppConfig` 的模板方法实现，包括设置配置值、批量更新多个配置、获取配置值等 |
| `AppConfig_utils.hpp` | `AppConfig` 的工具包装器实现，辅助模板类 `ConfigValue<T>`、`ConfigObject<T>` 的定义。`ConfigValue` 用于简单类型的配置项包装（带缓存和变更回调），`ConfigObject` 用于复杂结构体的配置包装，支持 JSON 序列化与验证。 |
| `ComboBoxDelegate.h` | **表格下拉框委托** （`ComboBoxDelegate`），用于规则表格的“通道”和“模式”列，提供 QComboBox 编辑器 |
| `ConfigManager.h` | 单个配置管理器 `ConfigManager` 的声明。封装了 JSON 配置文件的加载、保存、键值访问（支持默认值）、批量更新（`merge_patch`）、删除及变更通知（观察者模式）。内部使用递归互斥锁保证线程安全。 |
| `ConfigStructs.h` | 配置结构体的定义，包括通用的 `ConfigTemplate` 模板以及具体的 `MainConfig`、`SystemConfig`、`UserConfig` 结构体。每个结构体提供 `to_json`/`from_json` 静态方法用于 JSON 转换，以及 `validate()` 方法进行字段有效性验证。 |
| `Console.h` | Windows 控制台辅助类 `Console`（单例）的声明。用于在 GUI 程序启动时创建或附加调试控制台，设置 UTF‑8 代码页和字体，并重定向标准流。非 Windows 平台仅提供空实现。 |
| `DebugLog.h` | 日志系统核心类 `DebugLog`（单例）的声明。支持模块级日志等级过滤、多个输出接收器（sink）、线程安全写入。提供宏 `LOG_MODULE` 用于统一格式的日志输出。 |
| `DebugLog_utils.hpp` | 日志系统辅助工具，包含 `DebugLogUtil` 命名空间下的函数，如将 `QJsonValue` 转换为字符串、去除字符串中的换行符等，便于日志格式化。 |
| `DefaultConfigs.h` | 默认配置提供类 `DefaultConfigs` 的声明，仅包含静态方法 `get_default_config`，根据配置名称（如 "main"、"system"、"user"）返回对应的默认 JSON 配置。 |
| `DGLABClient.h` | Qt 主窗口类 `DGLABClient` 的声明，继承自 `QWidget`。负责界面初始化、按钮事件处理、日志显示控件管理、规则管理 UI（规则文件选择、表格展示、添加/编辑/删除规则），并通过 `PythonSubprocessManager` 异步调用 Python 子进程进行 WebSocket 连接/断开操作。样式系统方法 `apply_widget_properties()`、`apply_inline_styles()`，以及主题切换的增强。 |
| `DGLABClient_utils.hpp` | `DGLABClient` 的工具函数，目前包含 `contains_any_keyword` 辅助函数，用于在网卡名称中匹配黑/白名单关键字。 |
| `FormulaBuilderDialog.h` | **值模式编辑对话框** （`FormulaBuilderDialog`），带按钮快速插入符号、实时括号匹配检查和合法性验证  |
| `MultiConfigManager.h` | 多配置管理器 `MultiConfigManager`（单例）的声明。维护多个 `ConfigManager` 实例的注册表，支持按优先级（`__priority` 字段）排序配置，提供合并读取、优先级冲突检测、文件热重载等功能。 |
| `MultiConfigManager_impl.hpp` | `MultiConfigManager` 的模板方法实现，包括按优先级或名称获取/设置配置值的模板函数，以及内部排序缓存的管理。 |
| `PythonSubprocessManager.h` | Python 子进程管理器 `PythonSubprocessManager` 的声明。基于 `QProcess` 启动外部 Python 脚本，通过解析脚本输出的端口号建立 TCP 连接（`QTcpSocket`），实现 C++ 与 Python 的 JSON 通信。提供异步调用接口 `call`，支持超时和回调。 |
| `Rule.h` | 规则类 `Rule` 的声明。单个规则包含名称、通道（A/B）、模式（0-4）和带占位符 `{}` 的值计算式。提供占位符数量统计、通道规范化、值计算（支持四则运算和括号表达式）、生成命令以及用于 UI 显示的格式化字符串方法。 |
| `RuleManager.h` | 规则管理器 `RuleManager`（单例）的声明。负责扫描指定目录下的 JSON 规则文件（含特定关键字），加载/保存规则文件，管理当前规则集，提供规则的增删改查、命令生成（变参模板）以及显示字符串生成等接口。 |
| `RuleManager_impl.hpp` | `RuleManager` 的模板方法实现，主要提供 `evaluate_command` 变参模板函数，将参数转换为 `std::vector<int>` 后调用对应规则的生成方法。 |
| `SampledWaveformWidget.h` | 实时波形采样控件的声明。继承 `QWidget`，提供 `input_data(double)` 接口输入归一化数据，内部使用环形缓冲区存储采样点，并通过定时器驱动采样和重绘。支持设置采样间隔和最大振幅比例。 |
| `ThemeSelectorDialog.h` | 主题选择对话框的声明。继承自 `QDialog`，包含 `theme_selected` 信号和私有映射（中文名、模式名、主色）。通过 `setup_ui()` 构建网格布局的主题卡片，`create_theme_card()` 创建可点击的卡片控件，用于直观的主题选择。 |
| `ValueModeDelegate.h` | **值模式列自定义委托** （`ValueModeDelegate`），双击时弹出公式构建对话框，支持 `{}` + `+-*/()` 计算式  |

> **注**：`DGLABClient.ui` 为 Qt Designer 界面文件，与 `DGLABClient.h` 中的类关联，定义了主窗口的布局和控件。该文件虽不属头文件，但属于界面设计的一部分，应与头文件配套使用。

---

## 依赖关系

- **Qt 5/6**：`DGLABClient.h`、`PythonSubprocessManager.h`、`SampledWaveformWidget.h` 依赖 Qt Core、Widgets、Network 模块；`DebugLog_utils.hpp` 依赖 QtCore 的 JSON 类。
- **nlohmann/json**：所有配置相关头文件（`AppConfig.h`、`ConfigManager.h`、`ConfigStructs.h` 等）以及规则相关头文件（`RuleManager.h`）依赖 JSON 解析库。
- **C++20**：项目使用 C++20 标准，部分模板、概念（如 `ConfigSerializable`）要求编译器支持 C++20。

---

## 设计要点

### 1. 配置系统
- **多级配置**：通过 `MultiConfigManager` 管理多个 `ConfigManager` 实例（main/user/system），每个实例有独立优先级，读取时高优先级覆盖低优先级。
- **类型安全**：`ConfigValue<T>` 和 `ConfigObject<T>` 包装配置项，提供类型安全的读写、缓存和变更回调，避免直接操作 JSON。
- **热重载**：`MultiConfigManager` 支持文件监控，当配置文件变化时自动重新加载并通知监听器。

### 2. 日志系统
- **模块化过滤**：每个模块可独立设置日志等级，支持按等级输出或仅输出指定等级的日志。
- **多接收器**：可注册多个 `LogSink`（如控制台、UI 控件），每个接收器可设置独立的最低等级，日志消息会分发到所有符合条件的接收器。
- **线程安全**：所有日志写入操作均受互斥锁保护，确保多线程环境下的安全性。

### 3. Python 子进程通信
- **进程管理**：`PythonSubprocessManager` 通过 `QProcess` 启动独立 Python 进程，子进程启动后立即输出监听端口，主进程通过 `QTcpSocket` 连接。
- **异步调用**：提供 `call` 方法将命令提交到全局线程池执行，避免阻塞主线程；通过 `req_id` 关联请求与响应，支持超时和回调。
- **停止安全**：析构时设置停止标志，唤醒所有等待线程，并等待线程池任务完成，防止资源泄漏。

### 4. 规则引擎
- **模式匹配**：`Rule` 类使用 `{}` 作为占位符，可解析占位符位置并动态替换为整数参数，生成最终字符串。
- **文件管理**：`RuleManager` 扫描配置目录下含关键字的 JSON 文件，支持创建、删除、切换规则文件，并自动解析 `rules` 对象为 `Rule` 实例。
- **线程安全**：规则集合的读写操作使用互斥锁保护。

### 5. 波形采样控件
- **实时性**：使用独立 `QTimer` 定时采样，避免阻塞主线程；环形缓冲区无锁写入（通过索引取模），读取时加锁保护最新值。
- **灵活性**：支持运行时调整采样间隔和振幅比例，波形重绘自动响应。
- **可视化**：抗锯齿绿线折线，背景深色，基线灰色虚线，适应亮色/暗色主题（通过 QSS 设置背景色）。

### 6. GUI 响应性
- **后台任务**：`DGLABClient` 将耗时操作（如 Python 调用）通过 `PythonSubprocessManager::call` 放入线程池，完成后通过信号槽将结果传回主线程更新界面。
- **日志显示**：日志接收器 `qtSink` 将日志消息通过 `QMetaObject::invokeMethod` 安全地追加到 UI 控件，并支持按等级着色。
- **规则管理**：规则文件的加载、保存及规则的增删改查均在 UI 线程同步执行（操作轻量），不影响流畅度。
- **样式系统**：通过 `apply_widget_properties()` 为所有控件设置 `type` 属性，配合 QSS 中的 `[type="..."]` 选择器，实现统一的主题切换和视觉风格。

---

## 使用说明

### 包含头文件
项目中只需包含所需模块的头文件，例如：
```cpp
#include "AppConfig.h"
#include "DebugLog.h"
#include "RuleManager.h"
```

### 配置系统初始化
在应用程序启动时调用：
```cpp
AppConfig::instance().initialize("./config");
```
之后可通过 `AppConfig::instance().get_value<T>("key", default)` 读取配置。

### 日志输出
使用宏 `LOG_MODULE`：
```cpp
LOG_MODULE("MyModule", "myFunction", LOG_INFO, "Hello, world!");
```
可在 `DebugLog::instance()` 中注册自定义接收器，如 Qt 界面控件。

### Python 子进程调用
```cpp
auto* pyMgr = new PythonSubprocessManager(this);
pyMgr->start_process("python", "/path/to/Bridge.py");
// 异步调用
QJsonObject cmd{{"cmd", "connect"}, {"req_id", 123}};
pyMgr->call(cmd, [](const QJsonObject& resp){
    // 处理响应
}, 5000);
```

### 规则引擎使用
```cpp
// 初始化（通常在 AppConfig 初始化后调用）
RuleManager::instance().init();
// 加载规则文件
RuleManager::instance().load_rule_file("rules.json");
// 生成命令（参数将填充到 value_pattern 中计算）
QJsonObject cmd = RuleManager::instance().evaluate_command("strength_up", 2);
// cmd 包含: {"cmd":"send_strength", "channel":1, "mode":1, "value":2}
```

### 注意事项
- 所有 `*_impl.hpp` 文件通常被对应的 `.h` 文件在末尾包含，无需手动引入。
- 多线程环境下，确保对 `AppConfig` 的访问通过其提供的线程安全方法进行（内部已加锁）。
- Python 子进程的 `Bridge.py` 必须实现 JSON 行协议（每条响应以换行符结尾），并正确处理 `req_id`。
- 规则文件中的 value_pattern 可以包含 `{}` 占位符，调用 `evaluate_command` 时传入的参数数量必须与占位符数量匹配。
- 规则支持通道（A/B）和模式（0-4）配置，模式含义：0=递减, 1=递增, 2=设为, 3=连减, 4=连增。
