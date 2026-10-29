# 架构

DG-LAB-Client 是单体桌面应用，按层划分目录、依赖单向向下。本文说明分层、目录、数据流、已落地的架构决策与开发陷阱；功能与操作见 [features.md](features.md)，设备见 [devices.md](devices.md)。

## 项目定位

一个基于 Qt/QML 的 DG-LAB 桌面客户端。项目采用 C++20 编写，**应用自身托管 WebSocket 中转服务**（V3/V4）供 DG-LAB APP 扫码配对，无需 Node 或 Python 进程；同时支持郊狼 V3 蓝牙直连。实现了多级配置管理、模块化日志、波形库与可视化规则图（ComfyUI / UE 蓝图风格）等功能。版本号以 `config/main.json` 的 `app.version` 为唯一来源，构建时由 CMake 注入。

DG-LAB-Client 是一个为 DG-LAB（地牢实验室）设备设计的桌面客户端工具，核心目标是从外部程序（如游戏、传感器等）获取数据，经过可配置的规则引擎计算后，向 DG-LAB 的 WebSocket 服务发送强度调节、波形输出等指令。

项目采用 **C++20 + Qt6/QML** 实现，分为 `core`（配置/日志）、`module`（数值与插件）、`rule`（规则与规则图）、`wave`（波形）、`ble`（蓝牙）、`net`（WebSocket 中转）与 `ui`（界面桥接 + QML）七层，依赖单向。整体架构如下:

- **C++ 主程序**: 负责界面桥接、多级配置（main/system/user）、规则与规则图、波形库、日志系统与传输调度。
- **内置 WebSocket 中转服务**: 应用自身托管 DG-LAB 中转服务（V3 端口 9999 / V4 端口 9998），由 DG-LAB APP 扫码配对，应用直接下发强度/波形/清除指令并接收回传；配对二维码由内嵌 `qrcodegen` 生成，载荷按官方终端二维码协议拼成 `https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://<局域网 IP>:<V3 端口>/<终端 ID>`（恰好两个 `#`，SOCKET 地址后不带额外路径），缺少官网下载地址或 `DGLAB-SOCKET` 标签时 APP 无法识别。`Bridge.py` / `WebSocketCore.py` 已退出 WebSocket 链路（Python 仅保留给插件工具 `PathFinder.py`）。
- **规则引擎**: 支持加载带占位符 `{}` 的 JSON 规则文件，动态填入参数（例如从外部数据模块接收的数值），生成最终命令。规则可指定通道（A/B）、模式（0~4）和值计算表达式（支持四则运算、括号）。
- **数据扩展机制（插件化）**: 通过动态库插件（`module/`）从不同外部源获取数据——现已内置 CS2 GSI 插件（从 CS2 游戏读取状态数据），未来可扩展更多插件；主程序仅负责接收数值序列并按规则处理，不关心数据具体含义，实现灵活的社区扩展。

> **当前状态**: 界面已整体迁移到 QML；具备配置管理、可视化规则图、波形库与编辑器、内置 V3/V4 中转服务与配对二维码、郊狼 V3 蓝牙直连、数值模块与插件化数据接入（如 CS2 GSI）等能力。

## 界面层与配置层

- **QML 界面层** `Dglab` QML 模块包含主窗口、页面、组件与对话框，样式令牌集中在 `style/` 单例（间距/字号/断点/组件度量）与 C++ 的 `Theme` 单例（颜色，支持 17 套预设与自定义主/副色，令牌与对比度说明见 [docs/theme.md](theme.md)）；QML 默认不写信号处理器，交互由 C++ 的 `UiConnector` 按 `objectName` 集中连接（画布类手势为登记在案的例外）。详见 [src/ui/README.md](../src/ui/README.md)。

- **配置系统** 采用 `MultiConfigManager` 管理多个 JSON 配置文件（main/user/system），支持优先级覆盖、文件监听式热重载（真重读 + 既有一致性通知链）、配置变更监听。配置项通过 `ConfigValue<T>` 或 `ConfigObject<T>` 包装，提供类型安全访问和缓存。

## 目录结构

```
DG-LAB-Client/
├── .github/workflows/          # CI/CD：Windows / Linux / macOS 构建与发布
├── assets/                     # 界面静态资源（图片、图标）
├── cmake/                      # CMake 辅助脚本（Qt 部署）
├── config/                     # 默认配置：main / system / user 与 rules/（见 config/README.md）
├── docs/                       # 开发文档（主题令牌、预设配色与对比度）
├── include/                    # C++ 公共头文件，按 core / rule / module / wave / ble / net / plugin / ui 分层（见 include/README.md）
├── licenses/                   # 第三方许可证文本（GPLv2 / LGPLv3 / MIT）
├── module/                     # 动态库插件源码：example、gsi（见 module/README.md）
├── python/                     # PathFinder.py：供 CS2 GSI 插件查找游戏目录
├── screenshot/                 # 文档用截图（pages：当前界面四页）
├── src/                        # C++ 源文件，与 include/ 一一对应（见 src/README.md）
│   └── ui/qml/                 # QML 界面：MainWindow、pages/、home/、dialogs/、components/、style/
├── third_party/miniz/          # 内嵌 zip 归档读写库（MIT，单文件 amalgamation）
├── third_party/qrcodegen/      # 内嵌二维码生成库（MIT）
├── CHANGELOG.md                # 更新日志
├── CMakeLists.txt              # 主构建脚本（配套 CMakePresets.json、qt.cmake、cmake/）
├── CONTRIBUTING.md             # 贡献指南（配套 CodingStyle.md、CODE_OF_CONDUCT.md）
├── DGLABClient.qrc             # Qt 资源清单
├── LICENSE                     # 许可证（GPL-3.0-only）
├── NOTICE                      # 第三方组件声明
├── README.md                   # 本文件
└── main.cpp                    # 程序入口
```

## 数据流

数值到设备的走向是一条单向链，层与层之间只经由接口相连:

1. **入口**: 插件（`IPlugin`）经宿主注入的 `DataListener` 接收外部推送；蓝牙输入设备（爪印 / 灵猫）由驱动把通知帧解析成数值。
2. **数值**: `ModuleManager` 按查询周期调度，变化时发 `value_changed`，写入自动钳制到数值声明的范围。
3. **计算**: 规则引擎（`RuleManager` / `RuleGraph`）与事件引擎（`EventEngine` + `EventRuntime`）订阅数值变化，分别产出通道命令与每帧强度，两者都不反向依赖 UI。
4. **合并**: `DeviceController` 用 `ChannelOutputState` 取规则最近值与事件输出的较大值，按 `min(limit, safety)` 钳制，相同值不重复下发。
5. **输出**: 已连接设备同时收到命令——蓝牙 `IOutputDevice` 实现（郊狼 / 负鼠），或内置中转服务 `DglabRelayServer` 转给 DG-LAB APP。
6. **呈现**: `UiConnector` 按 `objectName` 把 QML 控件连到桥接对象，QML 自身不写信号处理器（画布类手势是登记在案的例外）。

`main.cpp` 是组合根：它装配配置、日志、模块管理器、事件运行时、设备控制器与桥接对象，事件层因此不需要认识界面层。

## 配置系统

系统包含三个配置文件，按优先级从低到高依次为:

- `main.json`: 主配置，优先级 0
- `system.json`: 系统配置，优先级 1
- `user.json`: 用户配置，优先级 2

高优先级的配置项会覆盖低优先级的同路径配置。 **注意: ** 即使有覆盖逻辑，仍不建议在不同配置文件中定义相同属性配置文件中 `__priority` 字段用于定义优先级，不可删除。

> 若配置文件丢失将从默认配置（`DefaultConfigs.cpp`）加载，并在程序运行时自动生成缺失的配置文件。👉 配置文件相关问题请查看 [常见问题 - 配置问题](faq.md#配置问题)

<details>
<summary> 示例 `main.json` </summary>

```json
{
    "__priority": 0,
    "app": {
        "name": "DG-LAB-Client",
        "version": "2.4.0",
        "debug": false,
        "log": {
            "console_level": 0,
            "only_type_info": false,
            "ui_log_level": 0
        },
        "ui": {
            "theme": "classic"
        }
    },
    "python": {
        "path": "python"
    },
    "rule": {
        "path": "./config/rules",
        "key": "rule"
    },
    "version": "1.0",
    "DGLABClient": "DG-LAB-Client"
}
```

</details>

**注意: ** 其中 `"version"` 与 `"DGLABClient"` 为检查字段，内容随意，但 **请勿删除或修改** 此字段。`app.ui.theme` 存储当前选中的主题模式。

配置热更是「重读 + 通知」：三个配置文件各自用 `QFileSystemWatcher` 监听文件与其所在目录，外部写入后去抖 300 ms，由同一个 `ConfigManager` 实例调用 `reload()` 整体替换内存数据，再沿既有 listener 链下发（`ConfigManager::notify_listeners` → `AppConfig` 的 `invalidate_caches` + `notify_config_changed("main"/"system"/"user"/"all")`）。`app.devices.<MAC>.*` 与 `app.channels.<A|B>.*` 都是普通配置键，外部手改后约 0.4 s 生效，界面与下发随之跟随；`DeviceController` 不再自带 mtime 快照与轮询定时器，`limits_for()` 直接读配置，订阅「all」变更后再比对并广播通道上限。`load()` 仍是幂等的首次加载，`AppConfig::reload_all()` 走 `reload()` 这条真重读链路。

三条边界必须一起看：**失败保留**——JSON 损坏、缺 `version` / `DGLABClient` 或端口越界时不动内存数据，记 WARN 并保留上一份有效配置，绝不把配置退化成空对象，也不改写磁盘上的坏文件；**自写抑制**——`save()` 记下写出的内容与时刻，紧随其后的监听事件读到相同内容就丢弃，避免「保存 → 重读 → 再通知」的循环；**删除后重建**——文件级 watch 在删除时会被 Qt 丢掉，目录级事件负责把文件与目录重新 `addPath` 回来。保存路径另做一次三方合并（基准是上次读写的内容）：只有应用与外部都改过的叶子保留内存值，外部新增的键并入内存，因此应用保存不会覆盖掉外部刚加的键。

## 关键架构决策

**主题与令牌**: 17 套预设，默认 `classic`（DefaultConfigs、出厂 `config/user.json` 与 ThemeManager 回退值三处统一）。颜色只走 `Theme` 令牌，QML 不出现颜色字面量；令牌清单见 [theme.md](theme.md)。

**插件侧车元数据**: 展示元数据放在 `<exe>/module/<dll主干名>/plugin.json`（源码侧 `module/<name>/meta/`，经 `dgl_add_plugin(META_DIR ...)` 部署）。展示名与模块标识分离，`IPlugin::name()` 仍是模块标识（规则引用依赖它，不得改）。

**事件模型**: 声明在侧车 `events`；触发由宿主按「已注册数值 + compare」判定；映射存 `user.json`；事件只产出数值，走向与强度由规则决定；事件不落 `enabled`（只由级联决定）；`dataType` 取 strength / percent / wave；无子选项事件用隐式子选项 id `default`。语义见 [event-mapping.md](event-mapping.md)。

**通道上限**: 通道上限与安全上限是通道级全局配置 `app.channels.<A|B>.{limit,safety}`，首次读取从旧 per-plugin 值迁移一次；界面显示与下发都取 `min(limit, safety)`。

**规则图**: 插件级规则图存 `config/rules/plugin_<插件>.json`，总图存 `config/rules/rules.json` 的 `graph` 字段；节点类型 `pluginOutput`（仅总图）与 `defaultOutput`（每插件图唯一、不可增删）；求值先插件图后总图；启用级联按 `channel:A/B` 与 `graph:top|plugin:<id>` 生效。见 [rule-graph.md](rule-graph.md)。

**设备分发**: 应用只做收集、处理与分发，向所有已连接设备同时下发，插件配置不选设备。

**连接状态口径**: 蓝牙 0/1 + App WebSocket 在线客户端数（`DglabRelayServer::client_count()` → `DeviceController::appClientCount`），任一连接即「已连接设备 ×N」。

**图标**: Font Awesome Free 7.3.1 两个字重入库 + `AppIcon` 组件；FA7 两字重共用族名，必须显式指定 `font.weight`（solid = Black），否则 solid-only 图标渲染成方框。

**波形库**: 官方 16 个波形是只读资源（`assets/waves/official_waves.json`），`Wave` 同时持有 `raw` 与两种协议的帧（`frames_v2` / `frames_v3`），`app.wave.protocol` 决定发送用哪套、缺帧拒发。见 [wave-data.md](wave-data.md)。

**波形导入导出**: JSON / `.pulse` / `.zip`（vendor miniz，读支持 store 与 deflate，含路径穿越与大小上限防护）。

**波形标签**: 标签库分两层（内置为代码常量、自定义存 `config/waves/tags.json`），官方波形的标签是用户侧叠加数据（`official_wave_tags` 段）。

**输出设备接口**: `IOutputDevice` 抽象 + 47L1 型号识别（未支持型号拒绝连接）+ 每通道输出目标 `app.output.<A|B>.target`；事件波形（`dataType=wave`）按设备实际写入帧数推进。

**默认设备与上下限**: 评选规则与读取链见 [devices.md](devices.md)。

## 开发常见陷阱

以下每一条都踩过，改动相关代码前先读一遍:

1. 位置器/布局（Column/Row/Grid/ColumnLayout…）的 `implicitWidth/implicitHeight` 是**只读**的；只有 Item/Rectangle 可写。写错会让整个页面类型不可用、主界面加载失败，而 qmllint 不报。
2. 用位置器作 `Flickable.contentHeight` 的来源时才可靠；读布局的 implicit 尺寸会得到 0 或时序不确定值。
3. 布局内子项用 `Layout.*` 附加属性，位置器内子项必须用 `width: parent.width` —— 换容器时要逐个核对。
4. 导航是 C++ 驱动的：`AppBridge::Page` + `UiConnector::nav_bindings()` 的 objectName 映射；QML 导航按钮不写 onClicked。页内按钮可以写。
5. 侧车 `banner` 只是文件名（QML 不要自己拼路径）：头图统一走 `image://modulebanner/<插件文件名>/<banner>`，由 `ModuleBannerImageProvider` 按 `<插件扫描目录>/<dll 主干名>/<banner>` 解析；带路径分隔符或文件不可读时返回空图，卡片与详情页保持空态。
6. 启用 CS2 插件会往本机 Steam 的 CS2 cfg 目录写 `gamestate_integration_dglab.cfg`（插件既有副作用，需提示用户）。
7. 用户自己的提交曾整段删掉 `is_newer_version` 函数体导致 LNK2019；接手大改动后先构建一次确认 HEAD 可编译。
8. 两个子代理同时构建同一 `build-agent` 会互相抢 exe（LNK1104）。同一产物只允许一个代理构建。
9. 截图脚本坐标要用**屏幕绝对坐标**（`SetCursorPos` 不收窗口相对偏移），且必须 DPI 感知。

## 相关文档

- [features.md](features.md)：功能与使用说明
- [devices.md](devices.md)：设备支持矩阵、默认设备与上下限
- [rules.md](rules.md)：规则文件格式与编辑器操作
- [rule-graph.md](rule-graph.md)：规则图节点与求值
- [event-mapping.md](event-mapping.md)：事件声明、映射与每帧合成
- [wave-data.md](wave-data.md)：波形数据结构、标签与导入导出
- [theme.md](theme.md)：主题令牌与预设
- [plugin-development.md](plugin-development.md)：插件接口与构建
- [build.md](build.md)：构建、测试、环境事实与验收清单
- [faq.md](faq.md)：常见问题
- [screenshots.md](screenshots.md)：截图清单
- [ui-optimization-plan.md](ui-optimization-plan.md)：阶段三前的界面分析快照（历史）
- [../include/README.md](../include/README.md)、[../src/README.md](../src/README.md)、[../src/ui/README.md](../src/ui/README.md)：分层与界面控件契约
