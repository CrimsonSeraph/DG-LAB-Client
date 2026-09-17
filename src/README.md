# 源代码目录 (src)

本目录包含项目的全部 C++ 源文件（`.cpp`），按功能分层组织；程序入口 `main.cpp` 位于项目根目录。

| 子目录 | 分类 | 说明 |
| --- | --- | --- |
| `core/` | 核心基础设施 | 配置系统（AppConfig / ConfigManager / MultiConfigManager / 结构体 / 默认配置）与日志系统（DebugLog、Console、LogExporter） |
| `rule/` | 规则引擎 | 规则实体与规则管理器（`Rule`/`RuleManager`），以及可视化规则图模型（`RuleGraph`） |
| `module/` | 数值模块 | 数值模型与调度（`ModuleValue`/`Module`/`ModuleManager`，同时是插件宿主）、通用数据接收器 `DataListener` |
| `wave/` | 波形 | 波形模型 `Wave`（V3 八字节帧）与波形库 `WaveLibrary` |
| `ble/` | 蓝牙 | 郊狼 V3 蓝牙直连 `CoyoteBleController` |
| `net/` | 网络 | 应用内 DG-LAB WebSocket 中转服务 `DglabRelayServer`（V3 / V4） |
| `ui/` | 界面层（C++ 侧） | 桥接对象（`AppBridge`/`DeviceController`/`HomeBridge`/`ModuleBridge`/`WaveBridge`/`RuleGraphBridge`/`LogBridge`）、主题令牌 `ThemeManager`、二维码 `QrImageProvider`、QML 连接集中点 `UiConnector`，以及 QML 文件 `qml/` |

> 说明：`src/` 下除分类子目录外不再存放散落文件。

---

## 目录结构

```text
src/
├── core/     # 配置系统 + 日志系统 + 工具类
├── rule/     # 规则引擎 + 规则图模型
├── module/   # 数值模块与插件宿主 + 数据接收器
├── wave/     # 波形模型与波形库
├── ble/      # 郊狼 V3 蓝牙直连
├── net/      # 应用内 WebSocket 中转服务（V3 / V4）
├── ui/       # 界面桥接对象 + QML 界面
└── README.md # 本说明文件
```

---

## 分层与依赖方向

依赖是单向的，避免循环依赖：

```text
core   ->  （不依赖其他层）
module ->  core
rule   ->  core, module
wave   ->  core
net    ->  core
ble    ->  core
ui     ->  core, module, rule, wave, net, ble
```

规则引擎产生命令时只依赖 `ui/DeviceController` 暴露的下发接口（由 `main.cpp` 注入），传输实现（`net` 的 V3/V4 或 `ble`）可替换而不影响规则层。

---

## 一、core/ —— 核心基础设施

配置系统与日志系统，是其他所有层的基础。

| 文件 | 描述 |
| --- | --- |
| `AppConfig.cpp` | 应用配置主类（单例）：初始化、读写（点分路径）、监听器、批量操作、导入导出，内部集成 `MultiConfigManager` |
| `ConfigManager.cpp` | 单个 JSON 配置文件的加载/保存/键值访问/合并补丁/删除与变更通知（递归互斥锁） |
| `MultiConfigManager.cpp` | 多配置文件管理：按 `__priority` 排序合并、优先级冲突检测、热重载 |
| `ConfigStructs.cpp` | `MainConfig`/`SystemConfig`/`UserConfig` 与 JSON 互转、字段校验 |
| `DefaultConfigs.cpp` | 默认配置提供（`main`/`system`/`user`） |
| `DebugLog.cpp` | 日志核心（单例）：模块级等级过滤、多输出通道、线程安全写入，`LOG_MODULE` 宏 |
| `Console.cpp` | Windows 调试控制台（UTF-8、字体、标准流重定向） |
| `LogExporter.cpp` | 自动日志（分片轮转、数量/大小上限）与手动日志导出，设置持久化到 `user.json` |
| `ProcessChecker.cpp` | 跨平台进程存在性检查（供 GSI 插件判断游戏是否运行） |

---

## 二、rule/ —— 规则引擎与规则图

| 文件 | 描述 |
| --- | --- |
| `Rule.cpp` | 单条规则：占位符解析、`valuePattern` 求值（QJSEngine，支持 JS 表达式）、模式钳位与命令生成 |
| `RuleManager.cpp` | 规则文件扫描/加载/保存、规则增删改查、引用索引、级联触发（含深度保护）、通道启用、结果推送 |
| `RuleGraph.cpp` | 可视化规则图：节点（规则 / 模块源 / 运算符 / 高级 / 通道输出）与连线模型、规则 JSON 双向转换、侧车文件读写 |

---

## 三、module/ —— 数值模块与插件宿主

| 文件 | 描述 |
| --- | --- |
| `ModuleValue.cpp` | 单个可查询数值：查询周期枚举、可选最小/最大值（写入自动钳制） |
| `Module.cpp` | 一个模块（一组数值）与通道挂载关系 |
| `ModuleManager.cpp` | 模块注册、以最短周期为基准的调度轮询、数值变化推送；插件宿主（扫描/加载/卸载、版本与依赖校验、日志转发、`IPluginHost` 能力） |
| `DataListener.cpp` | 通用数据接收器（TCP HTTP POST / UDP），按 `source+type` 分发，含解析器抽象 |

---

## 四、wave/ —— 波形

| 文件 | 描述 |
| --- | --- |
| `Wave.cpp` | V3 波形模型：段落（起止频率/强度）生成八字节帧、JSON 互转 |
| `WaveLibrary.cpp` | 波形库（`config/waves/waves.json`）：列表/保存/删除、A/B 通道当前波形、内置波形 |

---

## 五、ble/ —— 蓝牙直连

| 文件 | 描述 |
| --- | --- |
| `CoyoteBleController.cpp` | 郊狼 V3 蓝牙：扫描/连接、每 100ms 写 B0（强度 + 双通道波形）、BF 软上限与平衡参数、B1 强度回执与电量 |

---

## 六、net/ —— 应用内 WebSocket 中转服务

| 文件 | 描述 |
| --- | --- |
| `DglabRelayServer.cpp` | 应用内托管 V3（默认 9999）/ V4（默认 9998）中转服务：clientId 分配、bind 配对、心跳、断开通知、强度/波形/清除指令与回传解析；V4 维护被控方设备列表并下发 `device.op` |

---

## 七、ui/ —— 界面层（C++ 侧与 QML）

C++ 侧提供桥接对象与连接集中点，QML 只做绑定与渲染；界面层说明、objectName 契约与"信号处理器例外"见 [ui/README.md](ui/README.md)。

| 文件 | 描述 |
| --- | --- |
| `ThemeManager.cpp` | 主题令牌（14 套预设 + 自定义主/副色），注册为 QML 单例 `Theme` |
| `AppBridge.cpp` | 应用信息与页面导航、状态栏文本 |
| `DeviceController.cpp` | 启停内置中转服务、配对链接与二维码、强度/波形/清除下发与回传转发 |
| `HomeBridge.cpp` | 首页 A/B 通道强度、上限、启用状态、模块与规则摘要 |
| `ModuleBridge.cpp` | 模块页卡片、统一查询周期、选中模块数值 |
| `WaveBridge.cpp` | 波形库、A/B 当前波形与预览点、波形编辑器草稿 |
| `RuleGraphBridge.cpp` | 规则图节点/连线数据、模块数值面板、节点编辑与保存写回 |
| `LogBridge.cpp` | 界面日志收集、级别过滤与导出 |
| `QrImageProvider.cpp` | 由内嵌 `qrcodegen` 生成配对二维码（`image://dglabqr`） |
| `UiConnector.cpp` | 按 `objectName` 集中建立 QML↔C++ 连接（含动态委托热区扫描） |
| `qml/` | QML 界面：`MainWindow`、`pages/`、`home/`、`dialogs/`、`components/`、`style/` |

---

## 编译依赖

- **C++20** 或更高版本
- **Qt 6**（Core、Gui、Network、Qml、Quick、QuickControls2、QuickDialogs2、WebSockets、Bluetooth）
- **nlohmann/json**（配置与规则解析，仅头文件）
- **qrcodegen**（`third_party/qrcodegen`，MIT，二维码生成）
- **Python**（可选，仅 GSI 插件的 `PathFinder.py` 使用）
