<div align="center">
  <img src="assets/icon/dglab-client-256.png" width="128" alt="DG-LAB-Client icon"/>
  <h1>DG-LAB-Client</h1>
  <img alt="version" src="https://img.shields.io/badge/dynamic/json?url=https%3A%2F%2Fraw.githubusercontent.com%2FCrimsonSeraph%2FDG-LAB-Client%2Fmain%2Fconfig%2Fmain.json&query=%24.app.version&label=version&color=ff4d8d">
  <img alt="GitHub License" src="https://img.shields.io/github/license/CrimsonSeraph/DG-LAB-Client">
  <img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/CrimsonSeraph/DG-LAB-Client">
  <p>一个基于 Qt/QML 的 DG-LAB 桌面客户端 · 自托管 WebSocket 中转 · 郊狼 V3 蓝牙直连</p>
</div>

> **目录**
>
> - [一、项目简介](#一项目简介)
> - [二、功能特性](#二功能特性)
> - [三、依赖项](#三依赖项)
> - [四、快速开始](#四快速开始)
> - [五、自动化构建](#五自动化构建)
> - [六、使用说明](#六使用说明)
> - [七、项目结构](#七项目结构)
> - [八、截图](#八截图)
> - [九、编码规范](#九编码规范)
> - [十、FAQ](#十faq)
> - [十一、贡献指南](#十一贡献指南)
> - [十二、许可证](#十二许可证)
> - [十三、联系方式](#十三联系方式)

一个基于 Qt/QML 的 DG-LAB 桌面客户端。项目采用 C++20 编写，**应用自身托管 WebSocket 中转服务**（V3/V4）供 DG-LAB APP 扫码配对，无需 Node 或 Python 进程；同时支持郊狼 V3 蓝牙直连。实现了多级配置管理、模块化日志、波形库与可视化规则图（ComfyUI / UE 蓝图风格）等功能。版本号以 `config/main.json` 的 `app.version` 为唯一来源，构建时由 CMake 注入。

> 当前版本: `v2.4.0`

> 详细请查看: [更新日志](CHANGELOG.md)。

## 一、项目简介

DG-LAB-Client 是一个为 DG-LAB（地牢实验室）设备设计的桌面客户端工具，核心目标是从外部程序（如游戏、传感器等）获取数据，经过可配置的规则引擎计算后，向 DG-LAB 的 WebSocket 服务发送强度调节、波形输出等指令。

项目采用 **C++20 + Qt6/QML** 实现，分为 `core`（配置/日志）、`module`（数值与插件）、`rule`（规则与规则图）、`wave`（波形）、`ble`（蓝牙）、`net`（WebSocket 中转）与 `ui`（界面桥接 + QML）七层，依赖单向。整体架构如下:

- **C++ 主程序**: 负责界面桥接、多级配置（main/system/user）、规则与规则图、波形库、日志系统与传输调度。
- **内置 WebSocket 中转服务**: 应用自身托管 DG-LAB 中转服务（V3 端口 9999 / V4 端口 9998），由 DG-LAB APP 扫码配对，应用直接下发强度/波形/清除指令并接收回传；配对二维码由内嵌 `qrcodegen` 生成，载荷按官方终端二维码协议拼成 `https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://<局域网 IP>:<V3 端口>/<终端 ID>`（恰好两个 `#`，SOCKET 地址后不带额外路径），缺少官网下载地址或 `DGLAB-SOCKET` 标签时 APP 无法识别。`Bridge.py` / `WebSocketCore.py` 已退出 WebSocket 链路（Python 仅保留给插件工具 `PathFinder.py`）。
- **规则引擎**: 支持加载带占位符 `{}` 的 JSON 规则文件，动态填入参数（例如从外部数据模块接收的数值），生成最终命令。规则可指定通道（A/B）、模式（0~4）和值计算表达式（支持四则运算、括号）。
- **数据扩展机制（插件化）**: 通过动态库插件（`module/`）从不同外部源获取数据——现已内置 CS2 GSI 插件（从 CS2 游戏读取状态数据），未来可扩展更多插件；主程序仅负责接收数值序列并按规则处理，不关心数据具体含义，实现灵活的社区扩展。

> **当前状态**: 界面已整体迁移到 QML；具备配置管理、可视化规则图、波形库与编辑器、内置 V3/V4 中转服务与配对二维码、郊狼 V3 蓝牙直连、数值模块与插件化数据接入（如 CS2 GSI）等能力。

## 二、功能特性

- **QML 界面层** `Dglab` QML 模块包含主窗口、页面、组件与对话框，样式令牌集中在 `style/` 单例（间距/字号/断点/组件度量）与 C++ 的 `Theme` 单例（颜色，支持 17 套预设与自定义主/副色，令牌与对比度说明见 [docs/theme.md](docs/theme.md)）；QML 默认不写信号处理器，交互由 C++ 的 `UiConnector` 按 `objectName` 集中连接（画布类手势为登记在案的例外）。详见 [src/ui/README.md](src/ui/README.md)。

- **内置 WebSocket 中转服务** `DglabRelayServer` 基于 `Qt WebSockets` 在应用内托管 DG-LAB 中转服务（V3 / V4）：分配 clientId、处理 bind 配对与心跳、转发强度/波形/清除指令、解析 APP 回传；V4 额外维护被控方设备列表并下发 `device.op` / `device.op.clear`。配对二维码由内嵌 `qrcodegen`（MIT）经 `QrImageProvider` 提供给 QML：二维码载荷固定为官方终端二维码协议格式（`https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://<局域网 IP>:<V3 端口>/<终端 ID>`），端口取 V3（V4 端口继续监听，但不进入二维码），界面上二维码下方另给一行便于阅读的短地址。

- **配置系统** 采用 `MultiConfigManager` 管理多个 JSON 配置文件（main/user/system），支持优先级覆盖、热重载、配置变更监听。配置项通过 `ConfigValue<T>` 或 `ConfigObject<T>` 包装，提供类型安全访问和缓存。

- **规则引擎** 提供 `Rule` 类（支持 `{}`、`{id:xxx(名称)}`、`{rule:xx}` 占位符）和单例 `RuleManager`。可从指定目录下扫描 JSON 规则文件（含关键字 `rule`），加载规则集，并支持创建/删除/切换规则文件。规则支持启用状态（`enabled`）、多父级（通道 A/B 或规则引用）、唯一规则序号，值模式支持空值语义（任一引用为空时忽略该项计算），规则间可通过 `{rule:xx}` 引用结果并级联触发，父级为通道的规则结果会生成通道指令（`send_strength` 等）。

- **进程检查 (ProcessChecker)** 提供 `ProcessChecker` 静态工具类，跨平台查询指定名称的进程是否在运行（Windows 使用 `tasklist`，macOS/Linux 使用 `ps`），支持大小写敏感开关。用于 CS2 GSI 配置更新时判断游戏是否运行（配置仅游戏启动时加载，运行中更新需提示重启游戏）。

- **CS2 GSI 插件 (CS2GsiPlugin)** `CS2GsiPlugin`（`module/gsi/`）将原静态 `CS2GSIModule` 改造为符合 `IPlugin` 接口的动态库插件：通过 Python 工具 `PathFinder.py` 跨平台查找 CS 游戏目录，随机选取监听端口，按模块最小查询周期计算 `buffer`/`throttle` 并生成 `gamestate_integration_dglab.cfg`（路径记录到 `user.json` 的 `app.gsi` 下，经宿主配置接口读写）。插件经宿主共享 `DataListener` 接收 GSI 数据（HTTP POST），注册完整数值列表（个人状态类 17 项 + 团队/地图类 6 项，参照官方 GSI 规范），并实现**自身/队友数据归属区分**：首次收到有效数据时记录 `player.steamid` 为本地玩家基准，之后比较——一致为自身（个人状态类数值正常更新：血量/护甲/金钱/闪光/烟雾/燃烧/回合击杀/爆头/总伤害/装备价值/总击杀/助攻/死亡/MVP 等），不一致为队友（个人数值**不更新**，仅团队/地图类数值更新：CT/T 得分、连续失利次数、炸弹状态、地图阶段）。周期变化时自动更新配置；若 `cs2.exe` 运行中，经宿主 `notify` 弹出重启游戏提示。

- **首页看板** 首页由页头与两列看板组成：左列设备状态卡（连接、输出、中转地址与 A/B 通道启停）与 A/B 通道卡（已挂载模块名称与查询周期、直连规则名称与最近计算值），右列已启用插件、近期触发流水与 A/B 波形预览，窗口变窄时合并为单列。强度调节与波形选择由迷你窗与波形设置页提供，不在首页。

- **数值模块（Module）** 提供 `ModuleManager` 单例与 `ModuleValue`/`Module` 数据模型，管理可查询数值（参照 CS2 官方 GSI 规范，如 `health`、`armor`、`team_num`、`money` 等）。查询周期在插件中心页头的统一查询周期条设置（250 ms / 500 ms / 1000 ms / 2000 ms / 4000 ms，一次应用到全部数值）；调度器以所有数值中最短的查询周期为基准进行轮询，数值变化时通过 `value_changed` 信号推送，供规则引擎等下游消费。数值支持可选最小/最大值配置（写入自动钳制到范围）。各数值的名称、当前值、范围、底层字段名与查询周期在插件详情的「数值」页签查看。

- **插件系统（IPlugin）** 提供插件化模块接口 `IPlugin`（纯虚基类）：生命周期（`initialize`/`uninitialize`/`cleanup`/`can_unload`）、自描述（名称、版本、API 版本、能力标志、依赖列表）、线程安全声明与错误码返回；统一跨平台导出宏 `PLUGIN_EXPORT`（Windows `_WIN32` / GCC/Clang 可见性），约定 `extern "C"` 的 `create_plugin`/`destroy_plugin`/`get_plugin_api_version` 工厂导出，主程序加载时校验 API 版本（`PLUGIN_API_VERSION`）。插件日志通过回调转发由宿主统一记录（不直接使用 `LOG_MODULE`，类名/方法名自动为插件自身名称与函数名）；内存隔离约定插件自行 `new`/`delete`（宿主仅调用 `destroy_plugin`）。`ModuleManager` 作为插件宿主：启动时扫描 `<程序目录>/module/`（路径由 `config/main.json` 的 `app.module.path` 配置，`app.module.scan_load` 控制"扫描即加载"），通过 `QLibrary` 动态加载、校验 API 版本与依赖、注入日志回调与宿主上下文（`IPluginHost`：数值注册/写入、共享 `DataListener`），并管理加载状态（暂未加载/已加载/加载失败）；卸载时执行 `can_unload` 检查 → `uninitialize` → `cleanup` → `destroy_plugin`。社区插件模板（`docs/community_plugin_template/`）演示完整的接口实现、导出与侧车样例；已注册的插件（`gsi`/`warthunder`/`buckshot`）构建后自动复制到 `<程序目录>/module/` 供扫描加载，展示元数据侧车随动态库放入 `module/<目标名>/plugin.json`。

- **主题系统（QML）** 颜色集中在 C++ 单例 `Theme`（`ThemeManager`）：17 套预设 + 自定义主/副色，运行时按主/副色派生 `surface` / `surfaceAlt` / `border` / `textPrimary` / `accent` 等语义令牌，并按 WCAG 对比度下限逐主题校正；配置页提供预设网格与取色对话框切换，即时生效。令牌清单、预设色值与扩展方式见 [docs/theme.md](docs/theme.md)。

- **日志与调试** `DebugLog` 提供模块化日志等级控制，可输出到控制台、Qt 界面等不同的 `LogSink`；通过 `Console` 类可在 Windows 上创建调试控制台。

- **日志导出（自动 + 手动）** `LogExporter` 提供两类日志记录：**自动日志**在程序启动、配置系统加载完毕后自动开始记录运行日志（默认写入程序目录 `log/`，受导出级别/数量/大小限制，超限分片、退出或导出后自动清理多余日志）；**手动日志**在点击“导出日志”时写入手动目录（默认 `log/handle/`），仅应用级别过滤，不受数量与大小限制。自动与手动各有独立的级别过滤设置（导出级别、仅指定级别、范围、位置）：手动日志的导出级别 / 范围 / 目录在左侧「日志」页直接配置，持久化到 `user.json` 的 `app.log.manual` 下；自动日志的 `app.log.auto` 仍由配置文件维护，界面不暴露。

- **波形库** 波形模型同时持有 V2 与 V3 两套帧（另有设计层原始串 `raw` 与可再编辑的段落 `sections`）：官方开源示例的 16 个波形（呼吸、潮汐、连击、快速按捏、按捏渐强、心跳节奏、压缩、节奏步伐、颗粒摩擦、渐变弹跳、波浪涟漪、雨水冲刷、变速敲击、信号灯、挑逗1、挑逗2）作为只读资源 `assets/waves/official_waves.json` 随包发布，启动时加载、不可删除或改名；用户波形存在 `config/waves/waves.json`。发送协议由 `app.wave.protocol`（`v2` / `v3`，缺省 `v3`）决定，目标版本缺帧时拒发并提示，不回落另一版本。波形可在波形设置页导入导出：JSON（单个对象或数组，V2 与 V3 帧同时写出、缺帧写空数组）与 `.pulse`（单波形纯文本，内容为 `raw` 原文，`raw` 为空时拒绝导出）；导入遇重名可选覆盖 / 重命名 / 跳过（对整批生效），只有 `raw` 或只有一套帧的条目照常入库并标记不可发送。批量互操作可选择 zip：归档内每个波形一个 `.pulse`（文件名按波形名生成、非法字符替换并去重）另附 `index.json` 名称清单，读取同时支持 store 与 deflate，并对单文件 4 MiB / 总量 32 MiB 解压上限与路径穿越条目名做拦截。波形可带标签：内置标签是代码常量（随版本走、用户不可删），自定义标签独立存在 `config/waves/tags.json`，用户波形上的标签与波形一起存进 `waves.json`；官方 16 个波形的帧与名称只读，但它们同样可以打标签——官方波形的标签是用户侧叠加数据，单独存 `config/waves/tags.json` 的 `official_wave_tags` 段（波形名 → 标签数组），官方资源每次重建后按该段恢复；JSON 导入导出同样带 `tags` 数组（官方波形导出时一并写出），导入时出现的新标签自动登记进标签库，删除自定义标签会连同使用中的波形（含官方波形）一起清理。数据结构、存储格式、标签规则与导入导出规则见 [docs/wave-data.md](docs/wave-data.md)。

- **设备通信** 应用通过内置中转服务（`DglabRelayServer`）与 DG-LAB APP 扫码配对后直接下发强度/波形/清除指令；郊狼 V3 设备可经蓝牙直连，蓝牙已连接时规则输出与波形发送优先走蓝牙。原先的 Python `Bridge.py` / `WebSocketCore.py` 通信层已移除。
- **默认设备与每设备上下限** 设备唯一标识取蓝牙 MAC（设备侧固定，重连不变）。默认设备只在有输出能力的设备之间评选（爪印、灵猫是纯输入设备，不参与）：`app.output.default_device` 非空且该设备在线时用它，否则按「郊狼 > 负鼠 > 其它，同类型先连接者优先」自动评选，没有在线输出设备时没有默认设备；用户主动断开或掉线的设备只会离开在线集合，因此自动回到自动评选，配置键保留。首页看板的下拉是切换入口，迷你窗、看板强度/波形与事件波形下发都以默认设备为目标。上下限的读取只有一条链、集中在 `DeviceController::limits_for(mac, channel)`：`app.devices.<MAC>.{limit_a, safety_a, limit_b, safety_b}` → `app.channels.<A|B>.{limit, safety}` → 内置默认（`limit=200` / `safety=150`），界面与下发都用这条链算出的 `min(limit, safety)`，切换默认设备后立即跟随。
- **设备支持三态** 蓝牙扫描结果按型号标注三种状态：郊狼 V3（`47L121`）「已支持」；负鼠（`47L127`）与爪印 V1.1（`47L120`）「实验性」但驱动已实现、行可点选连接（爪印是纯输入设备，只上报按键与参数，不参与输出与默认设备评选）；灵猫（`47L124`）与型号未知的非 `47L1` 设备「实验性」但驱动尚未实现，点选给出「<型号> 驱动尚未实现，本版无法连接」的提示而不是假装可用；未登记的 `47L1` 前缀与固件过旧的爪印 V1.0（`47L120100`，行内提示需先升级固件）「暂不支持」，行不可点击。实验性设备驱动落地并首次连接成功会弹一次「此设备连接为实验性，功能未经完整验证」，是否已提示记在 `app.devices.<MAC>.experimental_notice_shown`。

- **自动更新** `UpdateManager` 从 GitHub Releases 的 `manifest.json` 检查新版本，按平台（Windows NSIS / macOS DMG / Linux AppImage）下载更新包并校验 SHA256；`UpdateBridge` 将检查 / 下载 / 安装状态与进度暴露给 QML，安装经 `confirmDialog` 二次确认后由平台脚本替换文件并自动重启。新进程带 `--updated` 参数启动并写入健康标记：macOS / Linux 脚本据此决定保留新版或回滚旧版；Windows 因 NSIS 直接覆盖安装目录不做回滚，失败时仅写失败标记。

- **跨平台构建** 基于 CMake，支持 Windows、Linux、macOS 等平台，并通过 GitHub Actions 自动构建和打包。

## 三、依赖项

### 1. 系统依赖

- **C++ 编译器**: 支持 C++20 标准（如 GCC 10+、Clang 12+、MSVC 2022）
- **Qt 6**: Core、Gui、Network、Qml、Quick、QuickControls2、QuickDialogs2、WebSockets、Bluetooth、Widgets（系统托盘菜单）、Concurrent
- **Python**（可选）: 仅 GSI 插件的 `PathFinder.py`（查找 CS 游戏目录）需要，不再用于 WebSocket 通信

> 推荐使用 VS2022/2026（MSVC）进行 Windows 开发，Linux/macOS 可使用 GCC 或 Clang。👉 遇到依赖问题？请查看 [常见问题 - 编译与运行](#编译与运行)

### 2. 第三方库

- **[nlohmann/json](https://github.com/nlohmann/json)** (版本 3.12.0) 用于 JSON 解析。CMake 会在配置时自动从 GitHub 下载单头文件到构建目录 `/include/nlohmann/`

### 3. Python 包依赖

**不再需要**。原先的 WebSocket 通信与二维码生成已改为应用内实现（`DglabRelayServer` + 内嵌 `qrcodegen`），无需安装 `websockets` / `qrcode`。仅在使用 CS2 GSI 插件时需要系统中有 Python 解释器运行 `PathFinder.py`。

## 四、快速开始

### 1. 获取源码

```bash
git clone https://github.com/yourusername/DG-LAB-Client.git
cd DG-LAB-Client
```

### 2. 配置 CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt -DPython_ROOT_DIR=/path/to/python
```

- 对于 Qt6，CMake 通常能自动找到；若使用 Qt5，请确保 `Qt5` 包可用。
- 可通过 `-DPYTHON_PACKAGES_DIR=path/to/site-packages` 指定要打包的第三方 Python 包目录（可选，供 CI 使用）。

> 👉 配置或编译失败？请查看 [常见问题 - 编译与运行](#编译与运行)

### 3. 编译

```bash
cmake --build . --config Release
```

### 4. 运行

编译完成后，可执行文件位于 `build/Release`（Windows）或 `build`（Linux/macOS）目录下。运行时需要确保以下目录与可执行文件同级:

- `config/`: 包含 `main.json`、`system.json`、`user.json`、规则子目录（`config/rules/`）与波形库目录（`config/waves/`，官方内置波形的只读资源在 `assets/waves/`）
- `python/`: 可选，仅 `PathFinder.py`（GSI 插件查找游戏目录用）
- `assets/`: 包含资源文件（如图片）
- Qt 运行时（DLL / 平台插件 / QML 模块）由构建后自动 `windeployqt` 部署到可执行文件目录

CMake 的 `POST_BUILD` 命令会自动复制这些目录到输出目录。

### 5. 打包（生成安装包）

```bash
cpack
```

生成的安装包将位于 `build/` 目录下（根据平台不同，可能是 .exe、.dmg、.deb 等）。

## 五、自动化构建

本项目已配置 GitHub Actions，每次推送到 `main` 分支或创建以 `v` 开头的标签时，会自动在 Ubuntu、Windows 和 macOS 上构建并打包，生成对应平台的安装包:

- **Windows**: NSIS 安装程序 (`.exe`) 和 ZIP 压缩包
- **macOS**: DMG 磁盘映像和 TGZ 压缩包
- **Linux**: DEB、RPM 包以及 AppImage（自包含可执行文件）

你可以在 GitHub 仓库的 **Actions** 页面下载最新构建产物，或从 **Releases** 页面获取正式发布的版本。

## 六、使用说明

### 1. 配置文件

系统包含三个配置文件，按优先级从低到高依次为:

- `main.json`: 主配置，优先级 0
- `system.json`: 系统配置，优先级 1
- `user.json`: 用户配置，优先级 2

高优先级的配置项会覆盖低优先级的同路径配置。 **注意: ** 即使有覆盖逻辑，仍不建议在不同配置文件中定义相同属性配置文件中 `__priority` 字段用于定义优先级，不可删除。

> 若配置文件丢失将从默认配置（`DefaultConfigs.cpp`）加载，并在程序运行时自动生成缺失的配置文件。👉 配置文件相关问题请查看 [常见问题 - 配置问题](#配置问题)

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

### 2. Python 脚本（可选）

- `python/PathFinder.py`: 跨平台查找 CS 游戏目录的工具，供 CS2 GSI 插件调用。
- 原 `Bridge.py` / `WebSocketCore.py` 已随内置中转服务上线而移除。

### 3. 规则引擎

规则引擎允许您通过 JSON 文件定义一系列带占位符 `{}` 的规则，在运行时传入参数动态生成字符串（例如构造 `send_strength` 命令的 JSON）。使用步骤如下:

- **规则文件存放**: 默认规则目录为 `config/rules/`（可通过 `rule.path` 配置修改）。目录下的 JSON 文件若文件名包含 `rule` 关键字（可通过 `rule.key` 配置），则会显示在 UI 的规则文件列表中。
- **规则格式**: 每个规则文件应包含一个 `"rules"` 对象，键为规则名称，值为规则对象。规则对象字段如下:

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | bool | 启用状态（默认 true），关闭后不参与计算 |
| `parents` | array | 父级列表：`"A"`/`"B"` 为通道父级（结果发送给该通道），整数为规则序号（结果推送给对应规则） |
| `mode` | int | 模式 0-4（递减/递增/设为/连减/连增） |
| `valuePattern` | string | 值计算表达式，支持 `{id:xxx(名称)}` 引用模块数值、`{rule:xx}` 引用规则结果、`{}` 外部占位符 |

> 兼容旧格式：未提供 `parents` 时使用 `channel` 字段（`"A"`/`"B"`/空）作为唯一父级。

<details>
<summary> 示例 `rules.json` </summary>

```json
{
    "rules": {
        "wave_short": "{\"cmd\":\"send_pulse\",\"channel\":1,\"pulses\":[\"01020304\"],\"duration\":{}}",
        "strength_up": "{\"cmd\":\"send_strength\",\"channel\":{},\"mode\":1}"
    }
}
```

</details>

- **规则占位符**: 模式中的 `{}` 会被按顺序替换为参数（旧式传参）；`{id:xxx(名称)}` 计算时通过 `ModuleManager` 查询对应数值（括号内为名称注释，界面显示为灰色）；`{rule:xx}` 引用序号为 xx 的规则计算结果。若任一引用为空值，本次计算被忽略（不推送、不发送）。

- **级联触发**: 模块数值变化时自动触发引用该数值的规则计算；规则计算完成后结果推送给所有父级——父级为通道时通过调用函数将结果发送给 Python 端，父级为规则且启用时同样触发计算（带深度保护防循环引用）。通道启用时整条调用链开始运转，关闭时停止，不影响其他分支。
- **UI 操作**: 在客户端的“配置”页面中，您可以:
    - 从下拉列表切换规则文件。
    - 新建/删除/保存规则文件。
    - 添加/编辑规则: 规则名称使用普通文本输入，“父级”（原“通道”列）和“模式”使用下拉选择，“值模式”双击后弹出公式构建对话框（支持按钮快速插入符号 + 实时括号合法性检查 + “显示可用数值”按钮插入 `{id:xxx(名称)}`/`{rule:xx}` 引用）。
    - 规则引用范围: “显示可用数值”中的规则列表包含除当前编辑规则外的所有规则（含父级为通道的规则），即一个规则可以有通道与其他规则的混合父级；父级含通道的规则是否启用由独立的通道启用变量决定，引用它不要求对应通道已启用。
    - 启用状态: 规则表格首列“启用”为勾选框，点击即可启用/停用规则（停用的规则不参与计算）。
    - 父级编辑: 点击“编辑父级”按钮弹出父级编辑对话框——通道父级单选（无/A/B，含通道唯一性去重），规则父级多选（勾选后对应规则的值模式将引用本规则 `{rule:xx}`，取消勾选则移除引用）；确认后自动保存规则文件。
    - 父级列显示: 通道父级显示 “A”/“B”，规则父级显示 “rule:xx,xx”（超出显示 “...”），混合显示 “A;rule:xx,xx”，无父级显示 “无”。
    - 模式列灰显: 父级全部为通道时正常显示模式；父级无通道时文本后加 “(不适用)” 并整列灰色；父级混合时加 “(部分不适用)” 并整列使用更接近主题文本色的灰色（仅影响界面显示，规则文件内容不变）。
    - 通道唯一性: 同一通道（A 或 B）仅允许一个规则作为直连父级——从文件加载时保留序号最小的规则，其余置为“无”；手动设置时保留最后设置的规则。

- **在代码中使用**:
    ```cpp
    #include "RuleManager.h"
    // 初始化（AppConfig 初始化后调用）
    RuleManager::instance().init();
    // 加载规则文件（默认为 rules.json）
    RuleManager::instance().load_rule_file("rules.json");
    // 评估规则
    std::string result = RuleManager::instance().evaluate("strength_up", 1);
    // 将 result 作为命令发送给 Python 子进程
    ```

> **注意**: 规则文件可能包含任意命令，不要加载不可信的 JSON 文件！

### 4. 数值模块

数值模块负责从本地数据源获取数值（如 CS2 GSI 的 `health`、`armor` 等），并传递给规则引擎计算。使用步骤如下:

- **插件中心**: 点击左侧导航栏的「插件中心」进入。页头下方是常驻的「统一查询周期」条：下拉选择 250 ms / 500 ms / 1000 ms / 2000 ms / 4000 ms，点「应用到全部数值」一次性改掉所有数值的查询周期，右侧显示当前基准周期。下方为插件卡片网格（两列，列表视图下一列），右侧常驻「已启用」栏，列出已启用的插件与「配置」入口。
- **插件卡片**: 卡片显示 banner、插件名、作者或描述与标签，下方状态行显示「运行中」或「未启用」，加载失败时显示「加载失败」；状态行右侧为「启用」/「停用」与「配置」按钮。点卡片本体或「配置」进入插件详情（同页替换网格，右侧已启用栏隐藏）。插件中心只列扫描到的动态库插件，内置模块不再单独成卡；顶部提供搜索（插件名 / 作者 / 标签）、「全部 / 游戏 / 外设」分类与网格 / 列表切换。
- **查看数值**: 在插件详情切到「数值」页签，逐项列出该插件已注册的数值：名称、底层字段名、当前值（尚未上报显示「暂无」）、范围（最小值 ~ 最大值，单端缺失显示 —，两端都没配置显示「范围未知」）与查询周期；未注册数值时显示空态提示。数值可配置可选最小/最大值（插件提供），**写入自动钳制到范围**——外部超范围数据（如 GSI 血量 150）会被限制为边界值（100）。
- **周期选项**: 250 ms、500 ms、1000 ms、2000 ms、4000 ms，下拉文案即毫秒数值，统一应用到全部数值。
- **数值变化推送**: 模块保留上次查询结果，数值未变化时不推送；数值变化时通过 `ModuleManager::value_changed` 信号推送，供规则引擎等消费。
- **调度机制**: 以所有数值中最短的查询周期为基准周期（最小 250ms），每经过一个基准周期查询一次；周期为基准周期整数倍的数值按对应倍率间隔查询（如基准 250ms 时，500ms 的数值每 2 次查询一次，2s 的数值每 8 次查询一次），周期设置变化时自动重建调度器。
- **数据源**: 未设置数据源时数值保持“未获取”状态（「数值」页签显示「暂无」），不产生模拟数值；通过 `ModuleManager::instance().set_data_source(callback)` 接入真实数据（如 CS2 GSI）后开始取值。规则中引用无数据的数值视为空值，忽略该次计算。
- **通用数据接收器 (DataListener)**: 负责统一接收外部推送的数据。单实例监听单个端口（支持 TCP HTTP POST 与 UDP 数据报两种协议），解析为 JSON 后按数据包携带的来源标识分发：数据包信封格式为 `{"source": "<模块名>", "type": "<信息类型>", "data": {...}}`，无信封字段时回退到监听器配置的默认来源（如 CS2 GSI 数据默认来源 `CS2 GSI`、类型 `gsi`）。各模块通过 `register_handler(source, type, callback)` 注册处理器，即可共享同一端口或多端口并行接收。
- **插件加载**: `ModuleManager` 启动时扫描插件目录（默认 `<程序目录>/module/`，可由 `app.module.path` 更改），扫描到的插件默认**不立即加载**，插件中心卡片显示为「未启用」；`app.module.scan_load` 设为 `true` 时扫描即加载（调试用）。插件加载流程：`QLibrary` 动态加载 → `get_plugin_api_version` 版本校验 → 依赖校验（`dependencies()` 需已加载）→ `create_plugin` 创建实例 → 注入日志回调与宿主上下文 → `initialize()`（插件在此通过 `IPluginHost` 注册数值与数据处理器）。加载失败时记录失败原因，插件中心卡片显示「加载失败」。

> 👉 规则引擎相关问题请查看 [常见问题 - 规则引擎问题](#规则引擎问题)

### 5. 首页看板

首页（`pages/HomePage.qml`）由页头与两列看板组成：左列设备状态与 A/B 通道卡，右列已启用插件、近期触发与波形预览；窗口变窄时合并为单列。

- **页头**: 显示输出可用性与蓝牙/通道状态；右侧「插件管理」进入插件中心、「规则编辑器」打开规则编辑器，下方「迷你窗」按钮打开迷你窗。
- **设备状态卡（`DashboardDeviceCard`）**: 连接设备数、蓝牙与 App 连接状态、输出可用徽标、中转地址、默认设备下拉（只列在线输出设备，选中即写 `app.output.default_device`，无在线输出设备时显示「无在线输出设备」），以及 A/B 通道行（状态徽标 未运行 / 运行中 / 无输出、强度与上限只读、「启动通道」「关闭通道」按钮）。
- **通道卡（`DashboardChannelCard`）**: 只读显示该通道已挂载的模块（名称 + 查询周期）与直连规则（名称 + 最近一次计算值，未计算显示 `--`），列表限高滚动；卡底提示强度调节与波形选择由迷你窗与波形设置页提供。
- **已启用插件卡（`DashboardPluginCard`）**: 显示已启用插件数量与列表（名称 + 加载状态），点击条目跳转插件中心。
- **近期触发卡（`DashboardTriggerFeed`）**: 列出最近 8 条触发记录（时间、通道、模块·事件·选项、强度增量），无记录显示「暂无触发」。
- **通道波形卡（`DashboardWaveCard`）**: 并排预览 A/B 已绑定波形的强度曲线（静态绘制）与强度/上限，未绑定显示「未绑定波形」；滚动播放在迷你窗提供。
- **迷你窗**: 首页页头的「迷你窗」按钮（或托盘菜单「显示迷你窗」）打开一个无边框、置顶、不进任务栏的小窗。顶部标题条可拖动，右侧「锁定」按钮锁定后拖动失效（按钮变「解锁」）并写入 `user.json` 的 `app.ui.mini_window.locked`；「×」只隐藏窗口。窗口位置记在 `app.ui.mini_window.{x,y}`，落在屏幕外时自动夹回可见区域；宽度取「小 / 中 / 大」三档（280 / 340 / 420 逻辑像素，记在 `app.ui.mini_window.size`），高度按内容计算并设上限，位置 / 尺寸 / 透明度 / 档位变化后 400 ms 合并写回配置，重启后恢复。窗内服务：A/B 强度大数字、上限与占比；A−/A+、B−/B+ 强度增减与每通道「启动通道 / 关闭通道」（与看板走同一套槽，关闭运行中的通道先确认）；透明度滑杆 0.3~1.0（`app.ui.mini_window.opacity`，窗口即时变透明）；近 5 条触发流水（最新在上，无记录显示「暂无触发」，列表限高可滚）；A/B 滚动波形（未绑定显示「未绑定波形」，按 100 ms 一帧循环播放）。
- **系统托盘常驻**: 启动后托盘区出现应用图标，菜单三项——「显示主窗口」「显示迷你窗」「退出」，双击图标恢复主窗口。主窗口点「×」只隐藏到托盘，进程与蓝牙 / 中转服务继续运行；真正退出走托盘菜单的「退出」（退出前保存配置）。系统托盘不可用时自动降级：主窗口关闭即退出程序，并在日志中记录一次告警。

### 6. 日志

日志等级通过配置文件 `app.log.console_level`、`app.log.ui_log_level` 控制（数值含义见下表）。您也可以在代码中使用 `LOG_MODULE` 宏输出日志:

```cpp
LOG_MODULE("MyModule", "my_function", LOG_INFO, "This is a log message.");
```

日志等级枚举:

- 0: DEBUG
- 1: INFO
- 2: WARN
- 3: ERROR
- 4: NONE

可通过 `DebugLog::set_log_sink_level("qt_ui", level)` 动态调整 UI 日志显示级别。

> 👉 日志不显示或等级不生效？请查看 [常见问题 - 通用问题](#通用问题)

#### 日志导出（自动 + 手动）

- **自动日志**: 程序启动、配置系统加载完毕后自动开始记录运行日志（`LogExporter` 注册日志输出通道），默认写入程序目录下的 `log/` 文件夹；单个日志超过大小上限时分片写入多个文件（视为一份）。保留份数 `export_retain_count` 会被读入设置，但清理调用尚未接线，当前不会自动删除旧文件。
- **手动日志**: 点击左侧「日志」页面的「导出日志」按钮，将界面日志按手动设置写入手动目录（默认 `log/handle/`），不受数量与大小限制，不参与清理。
- **导出设置**: 「日志」页第二行直接配置手动导出，无需额外弹窗，改动立即写盘:
    - **导出级别**: DEBUG / INFO / WARN / ERROR。
    - **导出范围**: 仅该级别 / 该级别及以上 / 该级别及以下（默认「该级别及以上」，回显配置现值而不是固定第一项）。
    - **导出目录**: 可填相对程序目录的路径（如 `./log/handle`）或绝对路径，点「应用目录」创建并保存；页面底部显示解析后的绝对路径。
- 手动设置持久化到 `user.json` 的 `app.log.manual.*`；自动日志设置（`app.log.auto.*`：导出级别 / 范围 / 目录 / 保留份数 / 大小上限）仍由配置文件维护，界面不暴露。

### 7. 调试控制台

在 Windows 上，如果配置文件中的 `app.debug` 为 `true`，程序启动时会自动创建一个调试控制台，用于显示详细的日志输出。**注意**：该控制台依赖 `#include <windows.h>`，仅在 Windows 平台可用，并且需要在配置文件中启用调试模式。首页也会输出日志到 Qt 界面，可按需选择查看。

### 8. IP 自动选择与手动选择

中转连接的地址按三级顺序确定：**手动选中地址优先**；没有手动选中时**按黑/白名单自动选择**；名单过滤后没有可用网卡时回退 `127.0.0.1`。

自动选择的判定顺序：候选网卡先过黑名单（网卡名或描述包含关键词即忽略），剩下的按枚举顺序取第一个；白名单命中的网卡优先返回。默认黑名单跳过 VMware / Hyper-V / WSL / Radmin VPN / TAP / 蓝牙等虚拟与 VPN 网卡，默认白名单优先「以太网 / ethernet / wlan / wi-fi / en0」。默认值写在 `IpSelector` 里，首次修改名单后整份名单落到 `user.json`。

界面上点「选择网卡 IP」（`configSelectIpButton`）打开 IP 选择对话框：左侧维护黑名单、右侧维护白名单（关键词 + 添加 / 逐条删除），下方列出过滤后的候选网卡（网卡名 / 描述 / 地址），点某一行即选中，「确定」把选中地址写回地址栏并立即刷新二维码载荷，「恢复自动选择」清掉手动选中并按名单重选一次。也可以直接在地址栏填写 IP 与端口，连接时读取并持久化。

持久化位置（`config/user.json`）：`app.network.ip_blacklist`、`app.network.ip_whitelist`、`app.network.selected_ip`，以及连接时写入的 `app.websocket.ip` / `app.websocket.port`。名单与手动选中地址**重启后仍生效**。

### 9. 主题切换

程序内置了 **17 套**预设主题，涵盖亮色、暗色及多种彩色风格（如炭黑甜粉、深海奶白、克莱因黄、午夜蓝、森野绿等）。颜色由 C++ 单例 `Theme` 提供，切换后 QML 属性绑定即时刷新，无需重启或重载样式表。用户可通过以下方式切换主题:

1. 打开“配置”页面的“主题”卡片，点击“选择主题”按钮，弹出预设主题对话框。
2. 对话框逐行列出所有预设，每行显示主色块、副色块、中文名称与英文模式名（如 `light`、`charcoal_pink`）。
3. 单击任意一行即调用 `Theme.applyPreset(mode)` 应用该主题，并把模式名保存到 `config/user.json` 的 `app.ui.theme`。
4. 需要自定义主/副色时点击“自定义主题”，通过取色对话框选择颜色后保存（`Theme.applyCustom`，持久化到 `app.ui.custom.primary` / `app.ui.custom.secondary`）。

> **注意**: 所有主题的令牌都会按 WCAG 对比度下限校正，浅色主题下也不会出现不可读文字；令牌清单、每套预设的主/副色与派生色值、扩展新主题的方法见 [docs/theme.md](docs/theme.md)。

### 10. 自动更新

- **检查更新**：左下角常驻面板的「检查更新」（`panelCheckUpdateButton`）先打开独立的更新对话框（`src/ui/qml/dialogs/UpdateDialog.qml`），再立即触发 `UpdateManager::check_for_updates()`，拉取 `https://github.com/CrimsonSeraph/DG-LAB-Client/releases/latest/download/manifest.json`，与编译期注入的 `APP_VERSION` 做点分段比较。
- **下载与校验**：`start_download()` 按当前平台（`windows` / `macos` / `linux`）从清单 `platforms` 条目取文件名与 SHA256，下载到临时目录并在完成后校验；不匹配立即删除并报错。
- **安装与重启**：`apply_update_and_restart()` 生成平台脚本——
    - Windows：`start /wait <installer> /S` 静默安装，再以 `--updated` 拉起主程序；不做回滚（NSIS 已覆盖旧文件），失败时写 `update_failed.flag`。
    - macOS：备份 `.app` → 挂载 DMG → 复制新 `.app` → 启动 → 失败回滚。
    - Linux：备份 AppImage → 替换 → `chmod +x` → 启动 → 失败回滚。
- **健康检查**：带 `--updated` 启动的进程会在 5 秒后写入健康标记；macOS / Linux 脚本据此删除备份或回滚旧版。
- **反馈**：检查结果（发现新版本 / 已是最新）、下载完成、安装前确认与错误均经 `UpdateBridge::toast_requested` 走 Toast，安装动作本身经 `confirmDialog` 确认。
- **更新对话框**：顶部显示当前版本、有新版本时补版本徽标；中部展示 `release_notes` 全文，长文本在容器内滚动，不设最大行数也不做 elide；错误以危险色就地显示；`downloading` 时显示下载进度；底部为检查 / 下载 / 重启并安装 / 查看完整更新说明 / 关闭。关闭对话框只收起窗口，下载与进度状态保存在 `UpdateBridge`，重新打开继续显示。关于页不再承载更新入口。

### 11. 插件开发指南

插件化模块系统允许将数值模块（如 CS2 GSI）实现为独立的动态库，由主程序动态扫描/加载/卸载，无需重新编译主程序。

#### 11.1 目录与现有插件

- **源码目录**: `module/`（每个插件一个子目录，如 `module/gsi/`、`module/warthunder/`、`module/buckshot/`）。
- **运行时扫描目录**: 默认 `<程序目录>/module/`，构建后插件 DLL 自动复制至此；路径可由 `config/main.json` 的 `app.module.path` 更改。
- **现有插件**: `module/gsi/`（CS2 GSI 插件）、`module/warthunder/`（战争雷霆遥测插件）、`module/buckshot/`（恶魔轮盘桥接插件）；社区模板见 [`docs/community_plugin_template/`](docs/community_plugin_template/README.md)。

#### 11.2 接口（IPlugin，`include/plugin/IPlugin.h`）

所有插件必须继承 `IPlugin` 并实现：

| 分组 | 方法 | 说明 |
| --- | --- | --- |
| 自描述 | `name()` / `version()` / `api_version()` | 插件名称（同时作为模块名与日志类名）、版本号、API 版本 |
| 自描述 | `capabilities()` / `dependencies()` | 能力标志（`ProvidesValues`/`ConsumesData`/`HasSettingsUi`）与依赖插件名列表（宿主加载前校验） |
| 线程安全 | `thread_safety()` | 声明调用线程要求（默认仅主线程） |
| 生命周期 | `initialize()` | 初始化：注册数值、注册数据处理器、启动服务（返回 `PluginError` 错误码） |
| 生命周期 | `uninitialize()` | 反初始化：注销数值与处理器、停止服务（与 initialize 对称） |
| 生命周期 | `cleanup()` / `can_unload()` | 资源清理 / 卸载前检查（拒绝时保持已加载） |
| 周期通知 | `on_host_period_changed()` | 宿主查询周期变化时被调用（可选实现） |

| 日志 | `set_log_callback()` / `log()` | 日志回调注入；插件用 `PLUGIN_LOG(this, level, ...)` 宏上报 |

数值通过 `ModuleValue` 构造可携带**可选最小/最大值**（如 `ModuleValue("health", "当前血量", QueryPeriod::QUARTER_SECOND, "m_iHealth", 0, 100)`，空表示无上下限）：插件详情「数值」页签按 `最小值 ~ 最大值` 显示范围（两端都没配置显示「范围未知」，单端缺失显示 —），写入时自动钳制到范围。

#### 11.3 动态库导出约定

插件动态库必须通过 `extern "C"` 导出以下三个符号（`include/plugin/plugin_export.h` 提供 `PLUGIN_EXPORT` 宏）：

```cpp
// 编译插件时定义 PLUGIN_BUILD（见 12.6 构建示例）
extern "C" {
PLUGIN_EXPORT int get_plugin_api_version();  // 返回 PLUGIN_API_VERSION，宿主加载时校验
PLUGIN_EXPORT IPlugin* create_plugin();      // 创建实例（插件内部 new）
PLUGIN_EXPORT void destroy_plugin(IPlugin*); // 销毁实例（插件内部 delete）
}
```

#### 11.4 宿主能力（IPluginHost，`include/plugin/PluginHost.h`）

宿主在加载插件后通过 `attach_host()` 注入宿主上下文，插件在生命周期方法中调用：

| 能力 | 方法 | 说明 |
| --- | --- | --- |
| 数值 | `register_module_values()` / `unregister_module()` / `set_value()` | 注册/注销模块、写入数值（变化触发规则计算） |
| 数据接收 | `listen_data()` / `register_data_handler()` / `unregister_data_handler()` / `stop_listening_data()` | 共享 `DataListener`：监听端口、按 (source, type) 注册处理器；无信封数据回退默认来源 |
| 配置 | `get_config_value()` / `set_config_value()` | 读写宿主合并配置（写入持久化到 user 配置，如 GSI 插件的 `app.gsi.*`） |
| 基础信息 | `base_period_ms()` | 当前调度基准周期（用于 throttle 等计算） |
| 通知 | `notify()` | 用户可见通知（宿主弹窗，如“需重启游戏”） |

数据包信封格式（自定义协议建议采用）: `{"source": "<模块名>", "type": "<信息类型>", "data": {...}}`；无信封数据（如 CS2 GSI）回退到监听器默认来源。

#### 11.5 规范要求

- **日志转发**: 插件内部**不直接使用** `LOG_MODULE`，统一使用 `PLUGIN_LOG(this, level, ...)` 宏（类名/方法名自动为插件名称与函数名），由宿主统一记录。
- **内存隔离**: 插件实例在插件内 `new`，宿主仅调用 `destroy_plugin` 销毁；**禁止跨模块 new/delete**、**禁止静态全局变量**。
- **错误处理**: 可能失败的操作返回 `PluginError` 错误码而非抛出异常。
- **命名**: 插件类名以 `Plugin` 结尾（如 `CS2GsiPlugin`）；导出函数使用标准三件套固定名。
- **ABI 兼容**: 插件必须与主程序使用同一工具链与同一 Qt 版本构建，否则无法加载。
- **API 版本**: 接口不兼容变更时递增 `PLUGIN_API_VERSION`，主程序加载时校验。

#### 11.6 构建示例（CMake）

插件统一通过 `cmake/Plugins.cmake` 的 `dgl_add_plugin(<目标名> ...)` 注册，参数三项：`SOURCES`（插件源文件，必填）、`QT_LIBS`（按需链接的 Qt 模块）、`META_DIR`（展示元数据侧车目录）。

```cmake
dgl_add_plugin(dgl_my_plugin
    SOURCES
        module/my/MyPlugin.h
        module/my/MyPlugin.cpp
    QT_LIBS
        Qt::Core
        Qt::Network        # 按需
        Qt::WebSockets     # 按需，WebSocket 类插件追加
    META_DIR module/my/meta
)
```

`META_DIR` 指向存放 `plugin.json` 的侧车目录：配置期解析为绝对路径，构建后连同动态库复制到 `<程序目录>/module/<目标名>/`，安装时同样随动态库落位；宿主按 `<插件目录>/<动态库主干名>/plugin.json` 读取。

include 路径（`include/plugin`、`include/module`）、`PLUGIN_BUILD` 编译定义、`ModuleValue` 编译、`PREFIX ""`（动态库主干名恒等于目标名，侧车目录名才与之对应），以及构建后复制到 `module/` 目录，全部由 `dgl_add_plugin` 处理，插件作者无需手写。

侧车 `plugin.json` 最小示例（字段均可选，缺失时回退到插件自描述的名称）：

```json
{
    "id": "my_plugin",
    "name": "我的插件",
    "version": "0.1.0",
    "author": "你的名字",
    "description": "一句话说明插件输出的数值",
    "banner": "",
    "tags": ["示例"]
}
```

#### 11.7 加载流程与状态

1. 主程序启动时扫描插件目录（`*.dll`/`*.so`/`*.dylib`），**默认只扫描不加载**，插件中心卡片显示「未启用」与「启用」按钮；`app.module.scan_load=true` 时扫描即加载（调试用）。
2. 加载流程: `QLibrary` 动态加载 → `get_plugin_api_version` 校验 → 依赖校验 → `create_plugin` → `attach_host` + `set_log_callback` → `initialize()`（插件在此注册数值与数据处理器）。
3. 卸载流程: `can_unload()` 检查 → `uninitialize()` → `cleanup()` → `destroy_plugin()` → `QLibrary::unload`。
4. 加载失败时记录原因，插件中心卡片显示「加载失败」。

#### 11.8 GSI 插件数据归属规则（module/gsi/）

CS2 GSI 插件按 `player.steamid` 区分数据归属：首次收到有效数据时记录为本地玩家基准，之后比较——**一致为自身**（个人状态类数值正常更新：血量/护甲/金钱/闪光/烟雾/燃烧/回合击杀/爆头/总伤害/装备价值/总击杀/助攻/死亡/MVP 等），**不一致为队友**（个人数值不更新，仅团队/地图类数值更新：CT/T 得分、连续失利次数、炸弹状态、地图阶段）。字符串字段按固定集合映射为数字（如 `team` CT/T→3/2、`bomb.state`→1-4、`map.phase`→0-6），不固定的值放弃该字段。

## 七、项目结构

<details>
<summary> 目录树 </summary>

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
├── NextStep.md                 # 后续计划
├── NOTICE                      # 第三方组件声明
├── README.md                   # 本文件
└── main.cpp                    # 程序入口
```

</details>

## 八、截图

<details>
<summary>点击展开查看 3 张界面截图 - 最后更新：2026-10-23</summary>

|                     首页                     |                      配置页                      |
| :------------------------------------------: | :----------------------------------------------: |
| ![main_page](screenshot/pages/main_page.png) | ![config_page](screenshot/pages/config_page.png) |

| 关于页 |
| :-: |
| ![about_page](screenshot/pages/about_page.png) |

> 截图说明与重新生成方式见 [`screenshot/README.md`](screenshot/README.md)。

</details>

## 九、编码规范

- **文件编码与换行**: UTF-8 without BOM，换行符使用 CRLF（Windows 风格），文件末尾保留一个空行。
- **缩进与空白**: 使用 4 个空格缩进（不使用 Tab），左括号采用 K&R 风格（与语句同行）。
- **命名规则**: 类名用 `PascalCase`，变量/函数名用 `snake_case`，常量/宏用 `UPPER_CASE`；Python 模块名使用 `PascalCase`。
- **导入顺序**: 自定义模块 > 第三方库 > 标准库，每组内按字母顺序排列。
- **注释规范**: 独占一行的注释 `//` 后跟空格，与代码同缩进；行尾注释与代码间隔一个 Tab，`//` 后跟空格。

详细的编码规范请参阅项目根目录下的 **[CodingStyle.md](CodingStyle.md)**。

> **注意**: 仅需最后一次提交/发布时确保符号规范即可，无需在日常开发中严格遵守。

## 十、FAQ

### 编译与运行

<details>
<summary><b>Q1: CMake 配置时找不到 Qt</b></summary>

**现象**:

```
Could not find a package configuration file provided by "Qt6" or "Qt5"
```

**解决方案**:

- 确保 Qt 已正确安装，并且 `CMAKE_PREFIX_PATH` 指向 Qt 的安装目录（例如 `C:/Qt/6.5.0/msvc2019_64`）。
- 或者设置环境变量 `Qt6_DIR` / `Qt5_DIR`。
- 在 Linux 上可以通过包管理器安装 Qt 开发包（如 `qt6-base-dev`），CMake 通常能自动找到。

</details>

<details>
<summary><b>Q2: 提示找不到 Python 解释器或模块</b></summary>

**现象**: 启动后日志提示找不到 Python 解释器或模块。

**解决方案**:

- 主程序自身不再运行 Python：WebSocket 通信与二维码生成均已改为应用内实现，`Bridge.py` / `WebSocketCore.py` 已移除，因此不再需要 `pip install websockets qrcode[pil]`。
- 只有 CS2 GSI 插件需要系统 Python：确认 `python`（Linux/macOS 下为 `python3`）在 PATH 中，或通过配置项 `python.path` 指定解释器路径。`python/PathFinder.py` 只依赖标准库，无需安装第三方包。 </details>

<details>
<summary><b>Q3: nlohmann/json 下载失败</b></summary>

**现象**: CMake 配置过程中报错，无法从 GitHub 下载 `json.hpp`。

**解决方案**:

- 检查网络连接，确保能够访问 `raw.githubusercontent.com`。
- 手动下载 [json.hpp](https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp) 并放入构建目录的 `/include/nlohmann/` 下（根据 CMake 输出路径调整）。
- 或者修改 `CMakeLists.txt`，改用本地已下载的头文件路径。

</details>

<details>
<summary><b>Q4: 编译时出现 C++20 相关语法错误</b></summary>

**现象**: 编译器报错如 `'std::span' is not a member of 'std'` 或要求 C++20 标准。

**解决方案**:

- 确认编译器版本: GCC ≥10、Clang ≥12、MSVC ≥2022。
- 在 CMake 配置时显式指定 C++ 标准: `-DCMAKE_CXX_STANDARD=20`。
- 若使用旧版 IDE（如 VS2019），需要升级到 VS2022 或安装支持 C++20 的工具集。

</details>

### 配置问题

<details>
<summary><b>Q5: 配置文件修改后不生效</b></summary>

**现象**: 修改了 `user.json` 中的某个值，但程序运行时使用的还是旧值。

**解决方案**:

- 确认修改的文件是正确的（注意优先级: `user.json` > `system.json` > `main.json`）。如果 `system.json` 或 `main.json` 中定义了相同路径的配置，它们会被覆盖，但不会删除。
- 重新执行 CMake 构建项目或手动将修改后的配置文件复制到构建目录的 `/config/` 下。
- 检查 JSON 格式是否正确（如多余的逗号），可以使用在线 JSON 校验工具。

</details>

<details>
<summary><b>Q6: 配置文件丢失后如何恢复？</b></summary>

**现象**: 误删了 `config/` 目录下的某个 JSON 文件，程序启动报错或使用默认值。

**解决方案**:

- 程序内置了默认配置（定义在 `DefaultConfigs.cpp`），丢失的文件会在运行时自动重新生成（前提是目录存在）。只需确保 `config/` 目录可写。
- 也可以从源码仓库中复制 `config/` 目录下的示例文件到运行目录。

</details>

<details>
<summary><b>Q7: 更改客户端所使用的端口遇到问题？</b></summary>

**更改方法**:

- 编译运行客户端后，打开 `配置` 页面，在端口中输入 **你需要更换的端口** 并点击右侧的 `确定`，若显示端口更改成功即完成更改。
- 在默认的配置文件 `system.json` 中 `app.websocket.port` 即是所用端口

> 若在项目根目录更改，请确保构建输出目录下的配置文件也应用更改！端口范围建议使用 1024~~65535 之间的数字，避免使用系统保留端口（0~~1023）。

**现象**

通过上述方式设置了端口，但应用的仍然是旧的或默认端口 `9999`。

**解决方法**

- 点击 `确定` 后根据 `错误信息` 更换端口再次点击 `确定` 尝试更改。
- 尝试重新执行 CMake 构建项目或手动自定义的 JSON 规则文件放入构建目录的 `config/` 下。

</details>

### 规则引擎问题

<details>
<summary><b>Q8: 规则文件在 UI 中不显示</b></summary>

**现象**: 将自定义的 JSON 规则文件放入 `config/rules/` 目录，但在客户端的规则页面中看不到。

**解决方案**:

- 检查文件名是否包含默认配置文件 `main.json` 中项 `rule.key` 指定的关键字（默认为 `rule`）。例如 `my_rules.json` 包含 `rule` 字样，可以被识别；`settings.json` 则不会。
- 确认规则文件的 JSON 格式正确，且根对象包含 `"rules", "version", "DGLABClient"` 键。
- 尝试重新执行 CMake 构建项目或手动自定义的 JSON 规则文件放入构建目录的 `config/rules/` 下。

</details>

<details>
<summary><b>Q9: 规则评估生成的命令不符合预期</b></summary>

**现象**: 调用 `RuleManager::evaluate_command("rule_name", args...)` 返回的 `QJsonObject` 中 `value` 字段的值不正确（如始终为 0 或未按公式计算），或者通道/模式与设置不符。

**可能原因及解决方案**:

| 原因 | 解决方法 |
| --- | --- |
| 传入的参数个数与规则中的 `{}` 占位符数量不匹配 | 检查规则模式中 `{}` 的个数，确保传入参数数量一致。若参数不足，表达式中的剩余 `{}` 不会被替换，导致求值失败（日志会输出“参数数量不匹配”错误）。 |
| 值表达式求值失败（如语法错误、除零等） | 查看日志中的 `Rule::computeValue` 错误信息，表达式错误会输出 `表达式求值失败: ...`。修正 `value_pattern` 中的表达式（例如确保运算符正确、括号匹配）。 |
| 计算结果被模式钳位规则修正 | 模式 2（“设为”）将值钳位到 [0, 200]；模式 3/4（“连减”/“连增”）钳位到 [1, 100]。如果期望的值超出范围，会被自动限制，这是正常行为。 |
| 通道无效（非 "A" 或 "B"） | `Rule::normalize_channel` 会将通道规范化为 "A"/"B"，无效通道（如 "C"、空字符串）会导致命令中使用默认通道 1（即 A）。检查规则定义中的 `channel` 字段是否为 "A" 或 "B"（大小写不敏感）。 |
| 模式值超出 0~4 范围 | 模式仅支持 0~4，若配置了其他值，行为未定义。请在规则文件中使用正确的模式编号。 |

**调试建议**:

- 启用 DEBUG 日志级别（`app.log.console_level = 0`），查看 `RuleManager` 和 `Rule` 模块输出的详细日志，包括加载的规则参数、表达式求值过程和结果。
- 使用 `RuleManager::get_rule_value_pattern("rule_name")` 获取原始表达式，手动验证计算逻辑。

</details>

### 中转服务与配对问题

<details>
<summary><b>Q10: 手机 App 无法连接到 WebSocket 服务</b></summary>

**可能原因及解决**:

| 原因 | 解决方法 |
| --- | --- |
| 手机与电脑不在同一局域网 | 确保手机连接与电脑相同的 Wi-Fi 网络 |
| 防火墙拦截了端口 | 在防火墙中放行 WebSocket 服务使用的端口（默认 9999） |
| 二维码中的地址错误 | 检查二维码中的 IP 是否为电脑的正确局域网 IP，不要使用 `127.0.0.1` 或 `localhost` |
| APP 提示无法识别二维码 | 二维码载荷必须是官方终端二维码协议格式：`https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://<局域网 IP>:<V3 端口>/<终端 ID>`，恰好两个 `#`、SOCKET 地址后不带额外路径；二维码下方那行短地址只供阅读，不参与扫码 |
| 服务未正常启动 | 查看页面状态栏与日志输出，确认中转服务已启动 |
| 某些 VPN 会拦截非代理流量 | 关闭 VPN |

</details>

<details>
<summary><b>Q11: 配对失败，提示 400 或 401</b></summary>

**错误码说明**:

| 错误码 | 说明                       |
| ------ | -------------------------- |
| 400    | 此 ID 已被其他客户端绑定   |
| 401    | 要绑定的目标客户端不存在   |
| 402    | 收信方和寄信方不是绑定关系 |
| 404    | 未找到收信人（离线）       |

**解决方法**: 检查二维码是否正确生成了 `clientId`，确保客户端与服务端的连接未中断，并确认没有重复绑定。

</details>

<details>
<summary><b>Q12: 消息发送失败，提示 405</b></summary>

**原因**: JSON 消息长度超过了 1950 字符，APP 会丢弃该消息。

**解决方法**: 将消息拆分为多条发送，确保每条消息长度在 1950 字符以内。

</details>

### 通用问题

<details>
<summary><b>Q13: 日志在 Qt 界面不显示或显示等级不对</b></summary>

**现象**: 程序运行时，左侧「日志」页的列表没有输出，或者只显示 ERROR 级别，而配置中设置了 INFO。

**解决方案**:

- 先看「日志」页右上角的「级别」下拉是否被调到了较高级别（它直接调用 `DebugLog::set_log_sink_level("qt_ui", level)` 并写回配置）。
- 检查配置文件中的 `app.log.ui_log_level` 值（0=DEBUG,1=INFO,2=WARN,3=ERROR,4=NONE）。例如需要显示 INFO 及以上，应设置为 1。
- 确认没有在代码中动态修改 UI 日志的 sink 级别（`DebugLog::set_log_sink_level("qt_ui", level)`）。
- 如果使用 `LOG_MODULE` 宏，确保模块名称和函数名称正确，且日志等级不低于全局设置。

</details>

<details>
<summary><b>Q14: Windows 调试控制台不出现</b></summary>

**现象**: 在配置文件中设置了 `"debug": true`，但程序启动时没有弹出黑色控制台窗口。

**解决方案**:

- 仅当编译为 Debug 配置且 `app.debug` 为 true 时才会创建控制台。Release 模式下可能被优化掉。
- 检查是否在 Windows 平台（该功能依赖 `<windows.h>`，Linux/macOS 无效）。
- 尝试以管理员身份运行程序。

</details>

<details>
<summary><b>Q15: IP 自动选择得到 127.0.0.1，但明明有真实网卡</b></summary>

**现象**: 自动选择的 IP 是 `127.0.0.1`，而电脑实际有有效的局域网 IP。

**解决方案**:

- 自动选择只取第一个「已启用且非回环」网卡的 IPv4 地址，不区分虚拟网卡（VMware / VirtualBox / Docker 等）与物理网卡。若选到的地址手机访问不到，请在“配置”页面的“连接”卡片中手动填写局域网 IP。
- 如果没有任何可用网卡，自动选择会回退到 `127.0.0.1`，此时手机无法扫码连接。

</details>

### 更新问题

<details>
<summary><b>Q16: 检查更新失败或下载更新包失败</b></summary>

**现象**: 点击"检查更新"后提示网络错误，或下载过程中失败。

**可能原因及解决**:

| 原因 | 解决方法 |
| --- | --- |
| 无法访问 GitHub | 确认网络可访问 `github.com` 与 `raw.githubusercontent.com`；如需代理，请为系统或 Qt 配置代理 |
| `manifest.json` 不存在 | 仅正式 Release 附带 `manifest.json`；预发布或开发构建无此文件，检查更新会失败，属正常 |
| SHA256 校验失败 | 下载文件损坏或被中间人篡改；删除临时文件后重试 |
| 当前平台无更新包 | manifest 的 `platforms` 中缺少对应平台条目，等待发布方补齐 |

</details>

<details>
<summary><b>Q17: 更新后程序未能自动重启或回滚</b></summary>

**现象**: 安装完成后程序没有自动拉起，或新版本启动失败但没有回滚。

**解决方案**:

- Windows：NSIS 安装包以 `/S` 静默执行；若安装目录无写权限，安装会失败。请以有权限的账户运行，或把程序安装到用户可写目录。
- macOS / Linux：更新脚本会先备份旧版本，新版本启动后 3 秒内写入、脚本 5 秒后检查（`--updated` 参数），脚本据此决定保留新版还是回滚。若新版本在 5 秒内崩溃，脚本会自动回滚并重新打开旧版。
- 检查临时目录下的 `dglab_update.bat` / `dglab_update.sh` 与日志，确认脚本是否被执行。
- 若回滚也失败，手动从备份文件（`.bak`）恢复。

</details>

## 十一、贡献指南

欢迎提交 Issue 和 Pull Request。在贡献前请确保:

- 代码遵循现有风格（缩进 4 空格，使用 `#pragma once`，命名规范）。更多细节请参考 [编码规范](#九编码规范) 及 **[CONTRIBUTING.md](CONTRIBUTING.md)**。
- 请务必使用 UTF-8 without BOM 编码提交代码。
- 添加或修改功能时更新相关文档（如 README.md）。
- 确保本地测试通过（编译通过，功能正常）。
- 对于较大的改动，请先开 Issue 讨论。

> 详细的贡献流程、代码提交规范及 Pull Request 指南，请查阅 **[CONTRIBUTING.md](CONTRIBUTING.md)**

## 十二、许可证

本项目自身源代码采用 **GNU General Public License v3.0 only**（GPL-3.0-only）开源。详情请参阅项目根目录下的 [LICENSE](LICENSE) 文件。

本项目依赖的第三方组件适用不同的许可证:

- **Qt 框架**（Core, Gui, Network, Qml, Quick, QuickControls2, QuickDialogs2, WebSockets, Bluetooth, Concurrent）: GNU Lesser General Public License v3.0（LGPLv3）
- **nlohmann/json**: MIT 许可证
- **Nayuki qrcodegen**（`third_party/qrcodegen`）: MIT 许可证
- **miniz 3.0.2**（`third_party/miniz`）: MIT 许可证（波形库 zip 批量导入导出的归档读写）

有关第三方许可证的完整声明和文本，请查看 [NOTICE](NOTICE) 及 `licenses/` 目录。

## 十三、联系方式

- 作者: [CrimsonSeraph]
- BiliBili: [浪天幽影(UID: 1741002917)](https://space.bilibili.com/1741002917?spm_id_from=333.1007.0.0)
- X: [𝒞𝓇𝒾𝓂𝓈𝑜𝓃𝒮𝑒𝓇𝒶𝓅𝒽✟升天✟(@CrimSeraph_QwQ)](https://x.com/CrimSeraph_QwQ)
- 项目主页: [https://github.com/CrimsonSeraph/DG-LAB-Client](https://github.com/CrimsonSeraph/DG-LAB-Client)
