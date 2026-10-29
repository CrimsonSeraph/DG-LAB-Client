# 源代码目录 (src)

本目录包含项目的全部 C++ 源文件（`.cpp`），按功能分层组织；程序入口 `main.cpp` 位于项目根目录。

| 子目录 | 分类 | 说明 |
| --- | --- | --- |
| `core/` | 核心基础设施 | 配置系统（AppConfig / ConfigManager / MultiConfigManager / 结构体 / 默认配置）与日志系统（DebugLog、Console、LogExporter） |
| `rule/` | 规则引擎 | 规则实体与规则管理器（`Rule`/`RuleManager`），以及可视化规则图模型（`RuleGraph`） |
| `module/` | 数值模块 | 数值模型与调度（`ModuleValue`/`Module`/`ModuleManager`，同时是插件宿主）、通用数据接收器 `DataListener` |
| `wave/` | 波形 | 波形模型 `Wave`（V3 八字节帧）与波形库 `WaveLibrary` |
| `ble/` | 蓝牙 | 输出设备接口 `IOutputDevice`、郊狼 V3 蓝牙直连 `CoyoteBleController`（兼扫描入口）、负鼠 V1 蓝牙直连 `OpossumBleController`，以及爪印 V1.1 / 灵猫 V1 两个输入侧直连 `PawPrintsBleController`、`CivetBleController` |
| `device/` | 设备数值桥 | 输入类设备通知 → 数值模块：`DeviceValueBridge`（爪印 → `paw` 模块、灵猫气压 → `civet` 模块，含两个模块的内置事件声明与气压阈值抑制） |
| `net/` | 网络 | 应用内 DG-LAB WebSocket 中转服务 `DglabRelayServer`（V3 / V4）与局域网地址选择器 `IpSelector` |
| `ui/` | 界面层（C++ 侧） | 桥接对象（`AppBridge`/`DeviceController`/`HomeBridge`/`ModuleBridge`/`WaveBridge`/`RuleGraphBridge`/`LogBridge`）、主题令牌 `ThemeManager`、二维码 `QrImageProvider`、QML 连接集中点 `UiConnector`，以及 QML 文件 `qml/` |

> 说明：`src/` 下除分类子目录外不再存放散落文件。

## 目录结构

```text
src/
├── core/     # 配置系统 + 日志系统 + 工具类
├── rule/     # 规则引擎 + 规则图模型
├── module/   # 数值模块与插件宿主 + 数据接收器
├── wave/     # 波形模型与波形库
├── ble/      # 输出设备接口 + 郊狼 V3 / 负鼠 V1 蓝牙直连 + 爪印 V1.1 / 灵猫 V1 输入侧直连
├── device/   # 设备数值桥（爪印 → 数值模块 paw，灵猫 → 数值模块 civet）
├── net/      # 应用内 WebSocket 中转服务（V3 / V4）与网卡地址选择器
├── ui/       # 界面桥接对象 + QML 界面
└── README.md # 本说明文件
```

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
| `UpdateManager.cpp` | 自动更新：拉取 GitHub Releases 的 `manifest.json`、点分段版本比较、按平台选择更新包并下载、SHA256 校验、生成并执行跨平台更新脚本 |

## 二、rule/ —— 规则引擎与规则图

| 文件 | 描述 |
| --- | --- |
| `Rule.cpp` | 单条规则：占位符解析、`valuePattern` 求值（QJSEngine，支持 JS 表达式）、模式钳位与命令生成 |
| `RuleManager.cpp` | 规则文件扫描/加载/保存、规则增删改查、引用索引、级联触发（含深度保护）、通道启用、结果推送 |
| `RuleGraph.cpp` | 可视化规则图：节点（规则 / 模块源 / 运算符 / 高级 / 通道输出）与连线模型、规则 JSON 双向转换、侧车文件读写 |

## 三、module/ —— 数值模块与插件宿主

| 文件 | 描述 |
| --- | --- |
| `ModuleValue.cpp` | 单个可查询数值：查询周期枚举、可选最小/最大值（写入自动钳制） |
| `Module.cpp` | 一个模块（一组数值）与通道挂载关系 |
| `ModuleManager.cpp` | 模块注册、以最短周期为基准的调度轮询、数值变化推送；插件宿主（扫描/加载/卸载、版本与依赖校验、日志转发、`IPluginHost` 能力） |
| `DataListener.cpp` | 通用数据接收器（TCP HTTP POST / UDP），按 `source+type` 分发，含解析器抽象 |
| `PluginMeta.cpp` | 插件展示元数据侧车（`plugin.json`）：读取名称/作者/简介/头图/版本/标签，字段全部可选，缺失或坏 JSON 时回退到既有展示名称 |

## 四、wave/ —— 波形

| 文件 | 描述 |
| --- | --- |
| `Wave.cpp` | 波形模型：`name` / `source` / `raw` / `sections` 与 V2、V3 两套帧；段落（起止频率/强度）生成 V3 帧、HEX 规范化与校验、JSON 互转。字段与规范见 [docs/wave-data.md](../docs/wave-data.md) |
| `WaveLibrary.cpp` | 波形库（`config/waves/waves.json`）：列表/保存/删除、A/B 通道当前波形；启动加载只读资源 `assets/waves/official_waves.json` 的 16 个官方波形（不可删除 / 改名 / 覆盖），发送帧按 `app.wave.protocol`（v2 / v3，缺省 v3）选择 |

## 五、ble/ 与 device/ —— 蓝牙直连与设备数值桥

| 文件 | 描述 |
| --- | --- |
| `CoyoteBleController.cpp` | 郊狼 V3 蓝牙：扫描/连接、每 100ms 写 B0（强度 + 双通道波形）、BF 软上限与平衡参数、B1 强度回执与电量；按蓝牙名前缀 `47L1xxxx` 识别型号（`kModelTable`：型号段 / 显示名 / 驱动是否已实现 / 是否经实机验证 / 输出能力 / 类型 / 行内提示覆盖；前缀先长后短，`47L120100` 排在 `47L120` 之前），未登记前缀标注「暂不支持」并拒绝连接；兼任扫描与连接入口，`47L127` 负鼠的连接转交 `OpossumBleController`、`47L120` 爪印的连接转交 `PawPrintsBleController`、`47L124` 灵猫的连接转交 `CivetBleController`（`47L120100` 固件过旧，标「暂不支持」并拒绝连接），界面上的连接状态、电量与强度读当前生效链路（郊狼优先，其次负鼠）；以 `IOutputDevice` 适配方法对外，接口信号由既有信号转发 |
| `OpossumBleController.cpp` | 负鼠振动控制器 V1（`OpossumBleController : IOutputDevice`，`device_type()=opossum`、`support_state()` 固定实验性）：每 100ms 写 B0（双通道强度字节）、B3 强度指令（`0xFF` 不改）、B2 屏幕强度同步（24 字节完整形态失败后回退 20 字节截断形态，再失败置「屏幕同步不可用」并只记一条 WARN）、B3 回执与电量；波形帧取郊狼 V3 帧的第 4~7 字节（频率丢弃，有损转换），非法帧跳过并静音；B0 无回执，每次写入后主动发 `outputWaveChanged`；D0 按键数据本版未接入，只记一次 DEBUG |
| `OpossumFrames.h`（include 侧） | 负鼠指令构造纯函数：`build_b0()`（越界整通道弃用）、`build_b3()`（`0xFF` 不改）、`build_b2()` / `build_b2_short()`（24 / 20 字节两形态）、`wave_bytes_from_v3_frame()`（郊狼 V3 帧 → 4 字节强度）；测试目标 `opossum_frames_test` |
| `PawPrintsBleController.cpp` | 爪印无线按钮传感器 V1.1 输入侧（纯 `QObject`，**不实现 `IOutputDevice`**，无实机验证按实验性）：GATT 与郊狼 V3 相同、没有电量服务；服务发现完成后**立即写 50 指令**（17 字节），以 `51` 消息为回执，无回执重试一次（共 2 次）后记 `LOG_ERROR` 并经 `errorOccurred` 提示，断连重连走同一条 `write_config()`；解析 `51/5A/5B/5C/D0/F1` 并转成 `deviceInfoReceived` / `triggerStarted` / `triggerCancelled` / `paramChanged` / `physicalDataReceived` / `angleThresholdsChanged` 信号，`F1` 阈值按 `/2` 展示 |
| `DeviceValueBridge.cpp` | 设备数值桥：注册 `paw` 模块数值（`paw_values` 定义表：`button` / `param` / `event_id` / `accel` / `angle_x,y,z` / `voltage` / `battery`，全部带钳制范围），把爪印信号写成数值（只经 `ModuleManager::set_value`），并给出该模块的内置事件声明 `button_pressed` / `button_released`（`compare=increase` / `decrease`，阈值 1）；`D0` 的 10Hz 全量数据默认不落值，只有 `app.devices.paw.d0_values` 为 true 时才写入；同时注册 `civet` 模块数值（`civet_values`：`pressure` 单位 0.01 kPa / `battery` / `raw`），灵猫气压每 100ms 上报一次但只有变化达到 `app.devices.civet.pressure_epsilon`（缺省 10，即 0.1 kPa）时才写 `pressure` / `raw`，其余丢弃并计数（`civet_suppressed()`），避免 10Hz 触发规则求值；`civet` 的内置事件声明是 `pressure_rise` / `pressure_drop` |
| `CivetBleController.cpp` | 灵猫边缘控制传感器 V1 输入侧（纯 `QObject`，**不实现 `IOutputDevice`**，无实机验证按实验性）：服务 `0x180C` / 写 `0x150A` / 通知 `0x150B`，另有电量 `0x180A` / `0x1500`；服务发现完成后**立即写 50 指令**（17 字节，`0xD0` 开启气压上报），协议没有定义 50 的回执，以**首帧 D0 视为生效**，无上报重试一次（共 2 次）后记 `LOG_ERROR` 并经 `errorOccurred` 提示，断连重连走同一条 `write_report_command()`；D0 解析按实际长度宽容（长度 ≥ 10 字节才取第 8、9 字节，不足 10 字节丢弃并计数），转成 `pressureReceived`，电量转成 `batteryReceived`；`resetPressureZero()` 写 66 指令（13 字节失败回退 14 字节，两次都失败只记 WARN） |
| `CivetFrames.h`（include 侧） | 灵猫协议纯函数：`build_report_command()` / `build_report_start()` / `build_report_stop()`（50 指令，17 字节）、`build_command66()` / `build_command66_wide()`（66 指令 13 / 14 字节两形态）、`parse_report()`（D0：头 + 第 8、9 字节小端，长度 ≥ 10 字节才解析）、`pressure_kpa()`（原始值 ÷ 100 = kPa）、`battery_percent()`（1 字节百分比）；测试目标 `civet_device_test` |
| `PawFrames.h`（include 侧） | 爪印协议纯函数：`build_config()`（50 指令，触发设置长度必须 15）、`trigger_settings_none()` / `trigger_settings_button()`（`0x00` / `0x05` 触发模式）、`button_settings_byte()`（bit7 抬起 / bit6 角度子模式 / bit5~3、bit2~0 消抖）、`parse_message()`（51/5A/5B/5C/D0/F1，长度与固定字节不符一律未识别）、`halve_threshold()`（F1 展示值）；测试目标 `paw_device_test` |

## 六、net/ —— 应用内 WebSocket 中转服务

| 文件 | 描述 |
| --- | --- |
| `IpSelector.cpp` | 局域网地址选择器：枚举候选网卡（已启用、正在运行、非回环，含网卡名 / 描述 / 地址）、黑名单过滤与白名单优先、手动选中地址，名单与选中地址持久化到 `user.json` 的 `app.network.*`；`active_ip()` 是 `DeviceController::detect_lan_ip()` 的唯一来源，无 Widgets 依赖 |
| `DglabRelayServer.cpp` | 应用内托管 V3（默认 9999）/ V4（默认 9998）中转服务：clientId 分配、bind 配对、心跳、断开通知、强度/波形/清除指令与回传解析；V4 维护被控方设备列表并下发 `device.op`；配对载荷由 `pairing_url()` 按官方终端二维码协议拼成（官网下载地址 + `DGLAB-SOCKET` 标签 + `ws://<ip>:<V3 端口>/<终端 ID>`），`pairing_url_display()` 另给界面展示用的裸 V3 地址，V4 端口继续监听但不进入二维码 |

## 七、ui/ —— 界面层（C++ 侧与 QML）

C++ 侧提供桥接对象与连接集中点，QML 只做绑定与渲染；界面层说明、objectName 契约与"信号处理器例外"见 [ui/README.md](ui/README.md)。

| 文件 | 描述 |
| --- | --- |
| `ThemeManager.cpp` | 主题令牌（17 套预设 + 自定义主/副色，按 WCAG 校正对比度），注册为 QML 单例 `Theme`；令牌与预设见 [docs/theme.md](../docs/theme.md) |
| `AppBridge.cpp` | 应用信息与页面导航、状态栏文本 |
| `DeviceController.cpp` | 启停内置中转服务、二维码载荷 `pairingUrl`（官方格式）与展示文本 `pairingUrlText`、强度/波形/清除下发与回传转发；事件输出（`set_event_output()`）与规则命令按通道合并——取较大值、按通道上限钳制、相同值不重复下发；设备侧只依赖 `IOutputDevice`（`attach_output()` 注入），并按通道选择输出目标：`outputTargets()` / `outputTarget()` / `setOutputTarget()` 读写 `app.output.<A|B>.target`（缺省 `coyote`），切目标时停旧目标波形、清事件与规则波形状态并重发当前强度 |
| `HomeBridge.cpp` | 首页 A/B 通道强度、上限、启用状态、模块与规则摘要 |
| `ModuleBridge.cpp` | 模块页卡片、统一查询周期、选中模块数值 |
| `WaveBridge.cpp` | 波形库、A/B 当前波形与预览点、波形编辑器草稿；`protocol` 读写 `app.wave.protocol`，发送按协议取 V2 / V3 帧，目标版本缺帧时写 `lastError` 并拒发（不回落另一版本） |
| `RuleGraphBridge.cpp` | 规则图节点/连线数据、模块数值面板、节点编辑与保存写回 |
| `LogBridge.cpp` | 界面日志收集、级别过滤与导出 |
| `QrImageProvider.cpp` | 由内嵌 `qrcodegen` 生成配对二维码（`image://dglabqr`）；载荷为官方终端二维码协议格式（约 117 字符，版本按内容自动提升到 7） |
| `ModuleBannerImageProvider.cpp` | 插件侧车头图提供者：按 `image://modulebanner/<插件文件名>/<头图文件名>` 解析 `<插件扫描目录>/<dll 主干名>/<头图文件名>`（扫描目录取 `ModuleManager::get_plugin_dir()`）；id 只接受两段纯文件名，含盘符、路径分隔符、`..` 或多余段一律返回空图，文件缺失或不是图片同样按空态处理 |
| `UiConnector.cpp` | 按 `objectName` 集中建立 QML↔C++ 连接（含动态委托热区扫描） |
| `UpdateBridge.cpp` | 更新界面桥接：检查 / 下载 / 安装状态、下载进度与错误；安装经 `confirmDialog` 二次确认后调用 `UpdateManager` |
| `qml/` | QML 界面：`MainWindow`、`pages/`、`home/`、`dialogs/`、`components/`、`style/` |

## 编译依赖

- **C++20** 或更高版本
- **Qt 6**（Core、Gui、Network、Qml、Quick、QuickControls2、QuickDialogs2、WebSockets、Bluetooth）
- **nlohmann/json**（配置与规则解析，仅头文件）
- **qrcodegen**（`third_party/qrcodegen`，MIT，二维码生成）
- **Python**（可选，仅 GSI 插件的 `PathFinder.py` 使用）
