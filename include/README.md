# 头文件目录（include）

本目录包含项目的全部公共头文件（`.h` 及模板实现 `.hpp`），按功能分层组织，与 `src/` 一一对应。

| 子目录 | 分类 | 说明 |
| --- | --- | --- |
| `core/` | 核心基础设施 | 配置系统（AppConfig / ConfigManager / MultiConfigManager / 结构体 / 默认配置）、日志系统（DebugLog、Console、LogExporter） |
| `rule/` | 规则引擎 | `Rule` / `RuleManager`（含模板实现）与规则图模型 `RuleGraph` |
| `module/` | 数值模块 | `ModuleValue` / `Module` / `ModuleManager`（插件宿主）与 `DataListener` |
| `wave/` | 波形 | `Wave` 与 `WaveLibrary` |
| `ble/` | 蓝牙 | `IOutputDevice`（输出设备接口）、`CoyoteBleController`（兼扫描入口）、`OpossumBleController` |
| `net/` | 网络 | `DglabRelayServer`、`IpSelector` |
| `plugin/` | 插件接口 | `IPlugin`、`PluginHost`、`plugin_export.h` |
| `ui/` | 界面层 | 桥接对象、`ThemeManager`、`QrImageProvider`、`UiConnector` |

> 说明：`include/` 下除分类子目录外不再存放散落文件（仅保留本说明文件）。

## 目录结构

```text
include/
├── core/     # 配置系统 + 日志系统
├── rule/     # 规则引擎 + 规则图
├── module/   # 数值模块 + 数据接收器
├── wave/     # 波形模型与库
├── ble/      # 输出设备接口 + 郊狼 V3 / 负鼠 V1 蓝牙直连
├── net/      # 应用内 WebSocket 中转服务与网卡地址选择器
├── plugin/   # 插件接口
├── ui/       # 界面桥接对象
└── README.md # 本说明文件
```

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
| `UpdateManager.h` | 自动更新：清单拉取、版本比较、平台包选择、SHA256 校验、更新脚本生成与执行 |

## 二、rule/

| 文件 | 描述 |
| --- | --- |
| `Rule.h` | 规则实体：占位符、`valuePattern`、父级（通道/规则）与模式 |
| `RuleManager.h` / `RuleManager_impl.hpp` | 规则文件管理、规则增删改查、级联触发、`evaluate_command` 模板 |
| `RuleGraph.h` | 规则图节点/连线模型与规则 JSON 双向转换 |

## 三、module/

| 文件              | 描述                                          |
| ----------------- | --------------------------------------------- |
| `ModuleValue.h`   | 数值模型（周期、最值、钳制、JSON 互转）       |
| `Module.h`        | 模块（数值集合与通道挂载）                    |
| `ModuleManager.h` | 数值调度、变化推送、插件宿主（`IPluginHost`） |
| `DataListener.h`  | 通用数据接收器与解析器接口                    |
| `PluginMeta.h`  | 展示元数据侧车（`plugin.json`）读取             |

## 四、wave/ · ble/ · net/

| 文件                        | 描述                           |
| --------------------------- | ------------------------------ |
| `wave/Wave.h`               | V3 波形模型（段落 → 八字节帧） |
| `wave/WaveLibrary.h`        | 波形库与 A/B 当前波形          |
| `ble/IOutputDevice.h`      | 输出设备抽象接口（连接状态、电量、强度下发与回显、波形下发与进度、软上限、三个接口信号） |
| `ble/CoyoteBleController.h` | 郊狼 V3 蓝牙直连（B0/BF/B1），实现 `IOutputDevice`；兼任扫描与连接入口，负鼠型号转交负鼠驱动 |
| `ble/OpossumBleController.h` | 负鼠振动控制器 V1 蓝牙直连（B0/B3/B2），实现 `IOutputDevice`，驱动支持程度固定为实验性 |
| `ble/OpossumFrames.h` | 负鼠指令构造纯函数（B0 / B3 / B2 两形态）与郊狼 V3 帧 → 负鼠强度字节转换 |
| `net/DglabRelayServer.h`    | 应用内 V3/V4 中转服务（含二维码载荷与展示文本接口） |
| `net/IpSelector.h`          | 局域网地址选择器（候选网卡枚举、黑/白名单、手动选中地址，QML 侧经 `device.ipSelector` 读取） |

## 五、plugin/

| 文件              | 描述                                                       |
| ----------------- | ---------------------------------------------------------- |
| `IPlugin.h`       | 插件纯虚基类与生命周期/自描述接口                          |
| `PluginHost.h`    | 宿主上下文接口（数值注册、数据接收、配置读写、通知、周期） |
| `plugin_export.h` | 导出宏与 API 版本                                          |

## 六、ui/

| 文件                 | 描述                                          |
| -------------------- | --------------------------------------------- |
| `ThemeManager.h`     | 主题令牌（QML 单例 `Theme`）                  |
| `AppBridge.h`        | 应用信息与页面导航                            |
| `DeviceController.h` | 内置中转服务、二维码载荷与展示文本、指令下发与回传 |
| `HomeBridge.h`       | 首页通道面板数据                              |
| `ModuleBridge.h`     | 模块页数据与操作                              |
| `WaveBridge.h`       | 波形库与波形编辑器草稿                        |
| `RuleGraphBridge.h`  | 规则图编辑器桥接                              |
| `LogBridge.h`        | 界面日志收集与导出                            |
| `QrImageProvider.h`  | 二维码图像提供者（载荷为官方终端二维码协议格式） |
| `ModuleBannerImageProvider.h` | 插件侧车头图提供者（`image://modulebanner`，id 只接受「插件文件名 + 纯文件名」两段） |
| `UiConnector.h`      | QML↔C++ 连接集中点                            |
| `UpdateBridge.h`     | 更新界面桥接（QML 上下文属性 `updateBridge`） |
| `TrayController.h`   | 系统托盘：图标与三项菜单（显示主窗口 / 显示迷你窗 / 退出）、主窗口关闭落点 |

## 关键协议与约定

以下内容原先是各头文件里的长注释，现集中在此，代码中只保留简短说明。

### 中转服务消息（`net/DglabRelayServer.h`）

| 版本 | 默认端口 | 配对模型 | 消息 |
| --- | --- | --- | --- |
| V3 | 9999 | 单控制方 ↔ 单被控方 | `bind` / `heartbeat` / `break` / `msg`（`strength-通道+模式+值`、`pulse-通道:[...]`、`clear-通道`） |
| V4 | 9998 | 控制方（clientId）+ 多个被控方（tid） | `hello` / `client_attached` / `controller_attached` / `message`（RPC，如 `device.op`） |

服务由应用自身托管，取代原先「Python Bridge + Node 官方 V2 后端」的链路：DG-LAB APP 扫码接入后，应用直接与 APP 配对并下发强度/波形指令。外部协议参考 `DG-LAB-OPENSOURCE/socket/v2/README.md` 与 `dglab-kit/README.md`。

二维码载荷由 `DglabRelayServer::pairing_url()` 按官方「终端二维码协议」拼成：`https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://<局域网 IP>:<V3 端口>/<终端 ID>`，恰好两个 `#`、SOCKET 地址后不带额外路径，缺少官网下载地址或 `DGLAB-SOCKET` 标签时 APP 无法识别。`pairing_url_display()` 返回同一条连接去掉前缀后的短地址供界面展示；`v4_pairing_url()` 保留（V4 端口仍在监听），但不出现在二维码里。

### 郊狼 V3 蓝牙（`ble/CoyoteBleController.h`）

- GATT：服务 `0x180C`（写 `0x150A`、通知 `0x150B`）；电量服务 `0x180A`（读/通知 `0x1500`）。
- `B0`：每 100ms 写入 —— 序列号 + 强度解读方式 + 双通道强度 + 双通道各 4 组频率/强度。
- `BF`：软上限与频率/强度平衡参数，**重连后必须重写**。
- `B1`：主机回传序列号 + 双通道实际强度。
- 型号识别：按蓝牙名前缀 `47L1xxxx` 分类，映射表是 `CoyoteBleController.cpp` 匿名命名空间的 `kModelTable`（型号段 / 显示名 / 驱动是否已实现 / 是否经实机验证 / 输出能力 / 类型标识）——`47L120` 爪印（未实现）、`47L121` 郊狼 V3（已实现且验证）、`47L124` 灵猫（未实现）、`47L127` 负鼠（已实现、未验证）。GATT 与郊狼 V3 相同的设备靠名字区分，未登记前缀一律标记「暂不支持（型号 xxx）」并在 `connectDevice()` 前拒绝连接，避免写入语义不匹配的指令；非 `47L1` 前缀沿用不限定型号的既有行为。驱动已实现但未验证的型号按「实验性」展示且可连接，行内提示区分「实验性驱动已实现，未经实机验证」与「暂不可连接（驱动尚未实现）」。
- 扫描与连接入口：扫描仍由本类统一做，界面上的连接状态、电量、强度与状态文案读的是当前生效链路（郊狼优先，其次负鼠）；负鼠型号的连接请求转交 `OpossumBleController`，连接负鼠前会先断开郊狼链路。同一时间只维持一条蓝牙链路。
- 输出设备接口：`IOutputDevice` 是上层唯一的设备依赖（连接状态、电量、`strength_a/b`、`wave_sending/frame/frames`、`set_strength` / `play_wave` / `stop_wave` / `apply_limits`，以及 `outputStateChanged` / `outputStrengthChanged` / `outputWaveChanged` 三个信号）。`CoyoteBleController` 以上述既有信号转发接口信号，发射时机与次数不变。

外部协议参考 `DG-LAB-OPENSOURCE/coyote/v3/README_V3.md`。

### 负鼠振动控制器 V1（`ble/OpossumBleController.h`）

- GATT 与郊狼 V3 相同：服务 `0x180C`（写 `0x150A`、通知 `0x150B`）；电量服务 `0x180A`（读/通知 `0x1500`）。
- `B0`：每 100ms 写入 20 字节 —— 头 + 7 字节 0 + A 通道 4 字节 + 4 字节 0 + B 通道 4 字节；每字节代表 25ms，量程 0~100，**任一字节越界则该通道 4 字节整条弃用**（写全 0 静音），另一通道不受影响。
- `B3`：3 字节 —— 头 + A 通道强度 + B 通道强度，量程 0~200，`0xFF` 表示该通道不改。相对增减（模式 0/1 与连续增减 3/4）按设备回传强度折算成绝对值后再钳制。
- `B2`：唯一用途是刷新设备屏幕上的通道强度显示（24 字节 = 头 + 21 字节固定段 + A + B）。文档同时标称写特征最长 20 字节，两者冲突：先按 24 字节写并等写入回执，失败（无写入回执能力 / 回执超时 / 服务错误）则截断到 20 字节重试一次，再失败则把 `screen_sync_available_` 置 false 并只记一条 WARN，B0 / B3 不受影响。界面经 `ble.screenSyncAvailable` 显示「屏幕强度同步不可用」。
- 通知：`B3` 为强度回执（含物理按键造成的改变），收到后立刻补发 B2；`D0` 为 16 字节按键状态集，本版未接入输入侧，只在 DEBUG 记一次说明。
- 波形：郊狼 V3 帧是「频率 4B + 强度 4B」，负鼠只要强度，因此取第 4~7 字节重打包成 B0 的 4 个强度字节，频率信息丢弃（有损转换）；帧不足八字节或非 HEX 时跳过该帧并静音。
- 波形进度：B0 无回执，每次 B0 写入后主动发 `outputWaveChanged`，事件波形按此节奏推进。
- 实验性：驱动已实现但**没有实机验证**，`support_state()` 固定返回实验性，连接成功后按 `app.devices.<MAC>.experimental_notice_shown` 弹一次性提示。

### V3 波形帧（`wave/Wave.h`）

一条 V3 数据为 8 字节 HEX，代表 100ms：

```text
[频率1][频率2][频率3][频率4][强度1][强度2][强度3][强度4]
```

前 4 字节为 4 个 25ms 子单元的频率（10~240），后 4 字节为对应强度（0~100）。

### 数据接收信封（`module/DataListener.h`）

数据包信封格式为 `{"source": "<模块名>", "type": "<信息类型>", "data": {...}}`；缺少信封字段时回退到默认来源（`set_default_source` / `set_default_type`）。本类单实例监听单个端口（TCP HTTP POST 或 UDP），解析为 JSON 后按 `source + type` 分发。面向单线程事件循环使用，处理器注册表不做线程安全保护。

### 规则图节点与连线（`rule/RuleGraph.h`）

| 节点类型   | 语义                                                                           |
| ---------- | ------------------------------------------------------------------------------ |
| `Rule`     | 规则节点：输入端口数量 = `valuePattern` 中 `{}` 的个数，带内容的 `{}` 显示名称 |
| `Module`   | 模块源节点：来自某个已启用模块的数值                                           |
| `Operator` | 基础运算节点：`+ - * /`（结果向下取整）                                        |
| `Advanced` | 高级节点：`abs` / `square` / `sqrt` / `expression`（自定义表达式）             |
| `Channel`  | 通道输出节点：`A` / `B`（单输入，同一通道只允许一个来源）                      |
| `pluginOutput`  | 插件输出节点：只在总规则图出现、无输入端口，带 `pluginId`（归属插件）与 `references`（恒为一条 `plugin:<插件标识>`），取值 = 对应插件图默认输出的求值结果 |
| `defaultOutput` | 默认输出节点：每个插件规则图恰好一个、单输入（本图唯一出口），带 `pluginId` 与 `references`（引用它的图标识，如 `["top"]`） |

求值顺序为先插件图（默认输出节点的上游子图）、后总规则图（插件输出节点 → 通道节点，通道启用时下发绝对强度）；口径与规则节点的 `valuePattern` 一致，详见 [docs/rule-graph.md](../docs/rule-graph.md)。

连线方向为「源（输出）→ 消费端（输入端口）」。规则节点被其它节点引用时写为 `{rule:序号}`，模块源节点写为 `{id:值ID(名称)}`，运算符/高级节点生成 JS 子表达式（`valuePattern` 由 QJSEngine 求值，天然支持 `Math.*`）。

### 界面连接约定（`ui/UiConnector.h`）

QML 不写信号处理器（`onClicked` / `Connections`），每个交互控件都有稳定的 `objectName`，加载完成后由 `UiConnector` 按 `objectName` 显式建立连接。详见 [src/ui/README.md](../src/ui/README.md)。

### 插件约定（`plugin/IPlugin.h`）

插件实例由插件内部 `new`，宿主仅调用 `destroy_plugin` 销毁；禁止跨模块 `new` / `delete`，插件内禁止使用静态全局变量。所有可能失败的操作返回错误码而非抛出异常。详见 [module/README.md](../module/README.md)。

### 更新清单与版本比较（`core/UpdateManager.h`）

清单固定从 `https://github.com/CrimsonSeraph/DG-LAB-Client/releases/latest/download/manifest.json` 拉取，结构为：

```json
{
  "version": "2.4.0",
  "release_notes": "...",
  "platforms": {
    "windows": { "file": "...", "sha256": "..." },
    "macos":   { "file": "...", "sha256": "..." },
    "linux":   { "file": "...", "sha256": "..." }
  }
}
```

## 依赖关系

- **Qt 6**：Core / Gui / Network / Qml / Quick / QuickControls2 / QuickDialogs2 / WebSockets / Bluetooth / Widgets（托盘菜单 `QSystemTrayIcon` / `QMenu`）
- **nlohmann/json**：配置、规则与规则图
- **qrcodegen**（`third_party/qrcodegen`）：二维码生成
- **C++20**：模板与概念要求（`ConfigSerializable` 等）

## 设计要点

- **分层单向依赖**：`core` 无依赖；`module`/`rule`/`wave`/`net`/`ble` 依赖 `core`；`ui` 依赖全部。规则层通过 `DeviceController` 完成下发，不反向依赖传输实现。
- **配置系统**：多级配置（main/system/user）优先级合并、类型安全包装、热重载与变更监听。
- **日志系统**：模块级过滤、多输出通道（控制台 / 界面 / 自动日志文件）、线程安全；界面通过 `LogBridge` 注册独立通道。
- **规则引擎**：`valuePattern` 由 QJSEngine 求值，因此支持任意 JS 表达式（规则图的高级节点即生成 `Math.*` 子表达式）；规则间通过 `{rule:xx}` 引用并级联触发，带深度保护。
- **传输与蓝牙**：`DglabRelayServer` 由应用自身托管中转服务（无需 Node/Python）；`CoyoteBleController` 按官方 V3 蓝牙协议直连设备。
- **界面层**：QML 只做属性绑定与渲染，交互由 `UiConnector` 在 C++ 侧按 `objectName` 连接；画布类手势为登记在案的例外。
