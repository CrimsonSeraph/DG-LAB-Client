# 头文件目录（include）

本目录包含项目的全部公共头文件（`.h` 及模板实现 `.hpp`），按功能分层组织，与 `src/` 一一对应。

| 子目录 | 分类 | 说明 |
| --- | --- | --- |
| `core/` | 核心基础设施 | 配置系统（AppConfig / ConfigManager / MultiConfigManager / 结构体 / 默认配置）、日志系统（DebugLog、Console、LogExporter） |
| `rule/` | 规则引擎 | `Rule` / `RuleManager`（含模板实现）与规则图模型 `RuleGraph` |
| `module/` | 数值模块 | `ModuleValue` / `Module` / `ModuleManager`（插件宿主）与 `DataListener` |
| `wave/` | 波形 | `Wave` 与 `WaveLibrary` |
| `ble/` | 蓝牙 | `CoyoteBleController` |
| `net/` | 网络 | `DglabRelayServer` |
| `plugin/` | 插件接口 | `IPlugin`、`PluginHost`、`plugin_export.h` |
| `ui/` | 界面层 | 桥接对象、`ThemeManager`、`QrImageProvider`、`UiConnector` |

> 说明：`include/` 下除分类子目录外不再存放散落文件（仅保留本说明文件）。

---

## 目录结构

```text
include/
├── core/     # 配置系统 + 日志系统
├── rule/     # 规则引擎 + 规则图
├── module/   # 数值模块 + 数据接收器
├── wave/     # 波形模型与库
├── ble/      # 郊狼 V3 蓝牙直连
├── net/      # 应用内 WebSocket 中转服务
├── plugin/   # 插件接口
├── ui/       # 界面桥接对象
└── README.md # 本说明文件
```

---

## 一、core/

| 文件 | 描述 |
| --- | --- |
| `AppConfig.h` / `AppConfig_impl.hpp` / `AppConfig_utils.hpp` | 配置主类与模板方法、`ConfigValue<T>` / `ConfigObject<T>` 包装 |
| `ConfigManager.h` / `ConfigManager_impl.hpp` | 单文件配置读写与变更通知 |
| `MultiConfigManager.h` / `MultiConfigManager_impl.hpp` | 多配置文件优先级合并与热重载 |
| `ConfigStructs.h` | `MainConfig` / `SystemConfig` / `UserConfig` |
| `DefaultConfigs.h` | 默认配置提供 |
| `DebugLog.h` / `DebugLog_utils.hpp` | 日志核心与工具函数（`LogSink` / `LOG_MODULE`） |
| `Console.h` | 调试控制台 |
| `LogExporter.h` | 自动/手动日志导出设置与导出、清理 |
| `ProcessChecker.h` | 进程存在性检查 |

## 二、rule/

| 文件 | 描述 |
| --- | --- |
| `Rule.h` | 规则实体：占位符、`valuePattern`、父级（通道/规则）与模式 |
| `RuleManager.h` / `RuleManager_impl.hpp` | 规则文件管理、规则增删改查、级联触发、`evaluate_command` 模板 |
| `RuleGraph.h` | 规则图节点/连线模型与规则 JSON 双向转换 |

## 三、module/

| 文件 | 描述 |
| --- | --- |
| `ModuleValue.h` | 数值模型（周期、最值、钳制、JSON 互转） |
| `Module.h` | 模块（数值集合与通道挂载） |
| `ModuleManager.h` | 数值调度、变化推送、插件宿主（`IPluginHost`） |
| `DataListener.h` | 通用数据接收器与解析器接口 |

## 四、wave/ · ble/ · net/

| 文件 | 描述 |
| --- | --- |
| `wave/Wave.h` | V3 波形模型（段落 → 八字节帧） |
| `wave/WaveLibrary.h` | 波形库与 A/B 当前波形 |
| `ble/CoyoteBleController.h` | 郊狼 V3 蓝牙直连（B0/BF/B1） |
| `net/DglabRelayServer.h` | 应用内 V3/V4 中转服务 |

## 五、plugin/

| 文件 | 描述 |
| --- | --- |
| `IPlugin.h` | 插件纯虚基类与生命周期/自描述接口 |
| `PluginHost.h` | 宿主上下文接口（数值注册、数据接收、配置读写、通知、周期） |
| `plugin_export.h` | 导出宏与 API 版本 |

## 六、ui/

| 文件 | 描述 |
| --- | --- |
| `ThemeManager.h` | 主题令牌（QML 单例 `Theme`） |
| `AppBridge.h` | 应用信息与页面导航 |
| `DeviceController.h` | 内置中转服务、二维码、指令下发与回传 |
| `HomeBridge.h` | 首页通道面板数据 |
| `ModuleBridge.h` | 模块页数据与操作 |
| `WaveBridge.h` | 波形库与波形编辑器草稿 |
| `RuleGraphBridge.h` | 规则图编辑器桥接 |
| `LogBridge.h` | 界面日志收集与导出 |
| `QrImageProvider.h` | 二维码图像提供者 |
| `UiConnector.h` | QML↔C++ 连接集中点 |

---

## 依赖关系

- **Qt 6**：Core / Gui / Network / Qml / Quick / QuickControls2 / QuickDialogs2 / WebSockets / Bluetooth
- **nlohmann/json**：配置、规则与规则图
- **qrcodegen**（`third_party/qrcodegen`）：二维码生成
- **C++20**：模板与概念要求（`ConfigSerializable` 等）

---

## 设计要点

- **分层单向依赖**：`core` 无依赖；`module`/`rule`/`wave`/`net`/`ble` 依赖 `core`；`ui` 依赖全部。规则层通过 `DeviceController` 完成下发，不反向依赖传输实现。
- **配置系统**：多级配置（main/system/user）优先级合并、类型安全包装、热重载与变更监听。
- **日志系统**：模块级过滤、多输出通道（控制台 / 界面 / 自动日志文件）、线程安全；界面通过 `LogBridge` 注册独立通道。
- **规则引擎**：`valuePattern` 由 QJSEngine 求值，因此支持任意 JS 表达式（规则图的高级节点即生成 `Math.*` 子表达式）；规则间通过 `{rule:xx}` 引用并级联触发，带深度保护。
- **传输与蓝牙**：`DglabRelayServer` 由应用自身托管中转服务（无需 Node/Python）；`CoyoteBleController` 按官方 V3 蓝牙协议直连设备。
- **界面层**：QML 只做属性绑定与渲染，交互由 `UiConnector` 在 C++ 侧按 `objectName` 连接；画布类手势为登记在案的例外。
