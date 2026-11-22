# 插件开发

插件把外部数据源接进宿主：宿主负责扫描、加载、调度与日志，插件只实现 `IPlugin` 接口。完整可编译的模板见 [community_plugin_template/README.md](community_plugin_template/README.md)，宿主侧的分层与组合根见 [architecture.md](architecture.md)。

## 插件系统

- **插件系统（IPlugin）** 提供插件化模块接口 `IPlugin`（纯虚基类）：生命周期（`initialize`/`uninitialize`/`cleanup`/`can_unload`）、自描述（名称、版本、API 版本、能力标志、依赖列表）、线程安全声明与错误码返回；统一跨平台导出宏 `PLUGIN_EXPORT`（Windows `_WIN32` / GCC/Clang 可见性），约定 `extern "C"` 的 `create_plugin`/`destroy_plugin`/`get_plugin_api_version` 工厂导出，主程序加载时校验 API 版本（`PLUGIN_API_VERSION`）。插件日志通过回调转发由宿主统一记录（不直接使用 `LOG_MODULE`，类名/方法名自动为插件自身名称与函数名）；内存隔离约定插件自行 `new`/`delete`（宿主仅调用 `destroy_plugin`）。`ModuleManager` 作为插件宿主：启动时扫描 `<程序目录>/module/`（路径由 `config/main.json` 的 `app.module.path` 配置，`app.module.scan_load` 控制"扫描即加载"），通过 `QLibrary` 动态加载、校验 API 版本与依赖、注入日志回调与宿主上下文（`IPluginHost`：数值注册/写入、共享 `DataListener`），并管理加载状态（暂未加载/已加载/加载失败）；卸载时执行 `can_unload` 检查 → `uninitialize` → `cleanup` → `destroy_plugin`。社区插件模板（`docs/community_plugin_template/`）演示完整的接口实现、导出与侧车样例；已注册的插件（`gsi`/`warthunder`）构建后自动复制到 `<程序目录>/module/` 供扫描加载，展示元数据侧车随动态库放入 `module/<目标名>/plugin.json`。

## 接口与约定

插件化模块系统允许将数值模块（如 CS2 GSI）实现为独立的动态库，由主程序动态扫描/加载/卸载，无需重新编译主程序。

### 1. 目录与现有插件

- **源码目录**: `module/`（每个插件一个子目录，如 `module/gsi/`、`module/warthunder/`）。
- **运行时扫描目录**: 默认 `<程序目录>/module/`，构建后插件 DLL 自动复制至此；路径可由 `config/main.json` 的 `app.module.path` 更改。
- **现有插件**: `module/gsi/`（CS2 GSI 插件）、`module/warthunder/`（战争雷霆遥测插件）；社区模板见 [`docs/community_plugin_template/`](community_plugin_template/README.md)。

### 2. 接口（IPlugin，`include/plugin/IPlugin.h`）

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

### 3. 动态库导出约定

插件动态库必须通过 `extern "C"` 导出以下三个符号（`include/plugin/plugin_export.h` 提供 `PLUGIN_EXPORT` 宏）：

```cpp
// 编译插件时定义 PLUGIN_BUILD（见下文「构建示例」）
extern "C" {
PLUGIN_EXPORT int get_plugin_api_version();  // 返回 PLUGIN_API_VERSION，宿主加载时校验
PLUGIN_EXPORT IPlugin* create_plugin();      // 创建实例（插件内部 new）
PLUGIN_EXPORT void destroy_plugin(IPlugin*); // 销毁实例（插件内部 delete）
}
```

### 4. 宿主能力（IPluginHost，`include/plugin/PluginHost.h`）

宿主在加载插件后通过 `attach_host()` 注入宿主上下文，插件在生命周期方法中调用：

| 能力 | 方法 | 说明 |
| --- | --- | --- |
| 数值 | `register_module_values()` / `unregister_module()` / `set_value()` | 注册/注销模块、写入数值（变化触发规则计算） |
| 数据接收 | `listen_data()` / `register_data_handler()` / `unregister_data_handler()` / `stop_listening_data()` | 共享 `DataListener`：监听端口、按 (source, type) 注册处理器；无信封数据回退默认来源 |
| 配置 | `get_config_value()` / `set_config_value()` | 读写宿主合并配置（写入持久化到 user 配置，如 GSI 插件的 `app.gsi.*`） |
| 基础信息 | `base_period_ms()` | 当前调度基准周期（用于 throttle 等计算） |
| 通知 | `notify()` | 用户可见通知（宿主弹窗，如“需重启游戏”） |

数据包信封格式（自定义协议建议采用）: `{"source": "<模块名>", "type": "<信息类型>", "data": {...}}`；无信封数据（如 CS2 GSI）回退到监听器默认来源。

### 5. 规范要求

- **日志转发**: 插件内部**不直接使用** `LOG_MODULE`，统一使用 `PLUGIN_LOG(this, level, ...)` 宏（类名/方法名自动为插件名称与函数名），由宿主统一记录。
- **内存隔离**: 插件实例在插件内 `new`，宿主仅调用 `destroy_plugin` 销毁；**禁止跨模块 new/delete**、**禁止静态全局变量**。
- **错误处理**: 可能失败的操作返回 `PluginError` 错误码而非抛出异常。
- **命名**: 插件类名以 `Plugin` 结尾（如 `CS2GsiPlugin`）；导出函数使用标准三件套固定名。
- **ABI 兼容**: 插件必须与主程序使用同一工具链与同一 Qt 版本构建，否则无法加载。
- **API 版本**: 接口不兼容变更时递增 `PLUGIN_API_VERSION`，主程序加载时校验。

### 6. 构建示例（CMake）

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

`banner` 有两种写法：

- **纯文件名**：图片与该插件的侧车同目录，宿主按插件安装目录读取，源形如 `image://modulebanner/<插件文件名>/<文件名>`。
- **绝对 http(s) URL**：宿主直接把 URL 当 `Image.source` 联网加载，适合不随插件分发的大图，例如版权属于游戏厂商的封面。

相对路径、`file://`、带路径分隔符或盘符的写法按无图处理。封面当前由插件仓库的 `banners/` 目录提供，不随 zip 分发：发布打包会过滤侧车目录下的图片与 `banner*` 文件，所以纯文件名只在本地构建目录里生效，分发出去的插件要靠插件市场卡片显示封面就得填绝对 https URL（市场条目的 `banner` 就取自侧车这个字段）。

### 7. 加载流程与状态

1. 主程序启动时扫描插件目录（`*.dll`/`*.so`/`*.dylib`），**默认只扫描不加载**，插件中心卡片显示「未启用」与「启用」按钮；`app.module.scan_load=true` 时扫描即加载（调试用）。
2. 加载流程: `QLibrary` 动态加载 → `get_plugin_api_version` 校验 → 依赖校验 → `create_plugin` → `attach_host` + `set_log_callback` → `initialize()`（插件在此注册数值与数据处理器）。
3. 卸载流程: `can_unload()` 检查 → `uninitialize()` → `cleanup()` → `destroy_plugin()` → `QLibrary::unload`。
4. 加载失败时记录原因，插件中心卡片显示「加载失败」。

### 8. GSI 插件数据归属规则（module/gsi/）

CS2 GSI 插件按 `player.steamid` 区分数据归属：首次收到有效数据时记录为本地玩家基准，之后比较——**一致为自身**（个人状态类数值正常更新：血量/护甲/金钱/闪光/烟雾/燃烧/回合击杀/爆头/总伤害/装备价值/总击杀/助攻/死亡/MVP 等），**不一致为队友**（个人数值不更新，仅团队/地图类数值更新：CT/T 得分、连续失利次数、炸弹状态、地图阶段）。字符串字段按固定集合映射为数字（如 `team` CT/T→3/2、`bomb.state`→1-4、`map.phase`→0-6），不固定的值放弃该字段。

## 插件可用的 C++ 能力

插件能用的 C++ 面只有 `include/plugin/`：`IPlugin` 接口、`IPluginHost` 宿主能力、`plugin_export.h` 导出宏，以及共享库 `dglab_plugin_sdk` 的两个 C ABI 入口（`PathFinder.h`、`PythonRunner.h`）。这一层是稳定 ABI，宿主只在必要时前进版本；其余头文件与 `src/**` 都是宿主内部实现，插件 include 进来之后会在宿主升级时以很难定位的方式崩。

| 目录 | 归属 | 插件可否使用 |
| --- | --- | --- |
| `include/plugin/` | 插件接口与 SDK 头文件，稳定 ABI | 唯一允许 include 的目录 |
| `include/module/ModuleValue.h` | 由 `PluginHost.h` 间接带入（`ModuleValue` 出现在接口签名里），随插件接口一起稳定 | 不直接 include，靠 `PluginHost.h` 带进来 |
| `include/{core,rule,wave,ble,net,device,event}` 等其它子目录 | 宿主内部实现 | 禁止 |
| `src/**` | 宿主内部实现 | 禁止 |

`dglab_plugin_sdk` 是与宿主同构的共享库（Windows 为 `dglab_plugin_sdk.dll`），随应用分发、与可执行文件同级；插件动态库对它只留一条链接依赖，运行时用的是宿主进程里那一份，所以不会出现「宿主一套查找规则、插件一套查找规则」。构建时在插件目标上补一行：

```cmake
target_link_libraries(dgl_my_plugin PRIVATE dglab_plugin_sdk)
```

`dgl_add_plugin` 已把 `include/plugin` 加入包含路径，源码里 `#include "PathFinder.h"` 即可。

### 查找游戏安装目录（`dglab_find_game_dir`）

```cpp
int dglab_find_game_dir(const char* game_id, DglabFindResult* out);
```

结果结构由调用方分配，SDK 只写定长成员：`int found` 是命中标志，`path[1024]` 与 `source[64]` 是两个定长缓冲（UTF-8 且 NUL 结尾）。支持的标识见 `src/plugin/PathFinder.cpp` 的 `kGameProfiles`：`cs2`、`csgo`（按 Steam 目录名 `Counter-Strike Global Offensive` 查找）、`warthunder`、`war-thunder`（`War Thunder`）。

| 返回码 | 值 | 含义 |
| --- | --- | --- |
| `DGLAB_FIND_FOUND` | 1 | 命中，`path` 与 `source` 已填写 |
| `DGLAB_FIND_NOT_FOUND` | 0 | 未命中，`found` 为 0 |
| `DGLAB_SDK_ERROR_ARGUMENT` | -1 | 参数无效（`out` 为空指针、`game_id` 为空） |
| `DGLAB_SDK_ERROR_TRUNCATED` | -2 | 路径超出 `path[1024]`，已按容量截断写入，此时 `found` 仍为 1 |
| `DGLAB_SDK_ERROR_UNKNOWN_GAME` | -3 | `game_id` 不在支持表内 |
| `DGLAB_SDK_ERROR_INTERNAL` | -4 | SDK 内部错误（异常已捕获，不穿过 ABI） |

`source` 说明路径是怎么命中的，SDK 按内建优先级逐档尝试：

| source | 命中档位 |
| --- | --- |
| `manual` | 显式覆盖：环境变量 `DGLAB_GAME_DIR_<大写下划线标识>`（如 `DGLAB_GAME_DIR_CS2`、`DGLAB_GAME_DIR_WAR_THUNDER`）指向存在的目录 |
| `registry` | Steam 安装根的注册表/常见位置 |
| `steam-vdf` | Steam 的 `libraryfolders.vdf` 登记的附加库 |
| `known-path` | 非 Steam 安装的常见安装根 |

最小用法（命中后先确认目录仍存在，截断码下 `path` 是裁过的）：

```cpp
#include "PathFinder.h"

#include <QDir>
#include <QString>

QString CS2GsiPlugin::find_cs_directory() {
    DglabFindResult found{};
    const int code = dglab_find_game_dir("cs2", &found);
    if (code == DGLAB_SDK_ERROR_ARGUMENT || code == DGLAB_SDK_ERROR_UNKNOWN_GAME ||
        code == DGLAB_SDK_ERROR_INTERNAL) {
        PLUGIN_LOG(this, PluginLogLevel::Warn, "查找游戏目录失败，错误码 " << code);
        return QString();
    }
    if (found.found == 0) {
        PLUGIN_LOG(this, PluginLogLevel::Info, "未找到 CS2 安装目录");
        return QString();
    }
    const QString path = QDir::cleanPath(QString::fromUtf8(found.path));
    if (!QDir(path).exists()) {
        // 命中来源已经失效（用户卸载或移动了游戏），不静默采用
        PLUGIN_LOG(this, PluginLogLevel::Warn, "SDK 返回的目录已不存在");
        return QString();
    }
    PLUGIN_LOG(this, PluginLogLevel::Info, "找到游戏目录（来源 " << found.source << "）");
    return path;
}
```

### 运行 Python 脚本（`dglab_run_python`）

```cpp
int dglab_run_python(const char* interpreter, const char* script_path, const char* args,
                     int timeout_ms, char* out_stdout, std::size_t out_stdout_cap,
                     char* out_stderr, std::size_t out_stderr_cap);
```

- `interpreter` 是解释器路径，为空时回退到 `python`（走 `PATH`）；插件一般从宿主配置 `python.path` 取值，见[随插件分发的 Python 脚本](#随插件分发的-python-脚本)。
- `args` 是传给脚本的参数字符串（UTF-8），按空白切分，双引号包住的部分保持整体，`script_path` 恒为第一个参数；可以传空指针。
- `timeout_ms` 传 0 或负值取默认值 30000 ms。
- stdout / stderr 各是一块调用方分配的缓冲，容量是**含结尾 NUL 的容量**；容量传 0 时对应指针可以传空指针（只关心退出码的场合）。

| 返回码                         | 值   | 含义                                                    |
| ------------------------------ | ---- | ------------------------------------------------------- |
| `DGLAB_PYTHON_OK`              | 0    | 成功，stdout 已写入                                     |
| —                              | 正数 | 子进程退出码，脚本自身失败                              |
| `DGLAB_PYTHON_ERROR_ARGUMENT`  | -1   | 参数无效（`script_path` 为空，或容量大于 0 但缓冲为空） |
| `DGLAB_PYTHON_ERROR_TRUNCATED` | -2   | 输出超出缓冲，已按容量截断写入                          |
| `DGLAB_PYTHON_ERROR_TIMEOUT`   | -3   | 超时，进程已被 kill 并回收                              |
| `DGLAB_PYTHON_ERROR_START`     | -4   | 解释器启动失败                                          |
| `DGLAB_PYTHON_ERROR_CRASH`     | -5   | 子进程异常退出（崩溃）                                  |
| `DGLAB_PYTHON_ERROR_INTERNAL`  | -6   | SDK 内部错误（异常已捕获，不穿过 ABI）                  |

最小用法（解释器取宿主配置，脚本来自插件目录下的 `python/`）：

```cpp
#include "PluginHost.h"
#include "PythonRunner.h"

#include <QByteArray>
#include <QCoreApplication>
#include <cstddef>
#include <string>

// 运行 <插件目录>/python/read_stats.py，参数由调用方拼好
std::string MyPlugin::run_script(const std::string& args) {
    const std::string interpreter = host_->get_config_value("python.path", "python");
    const QString script_path = QCoreApplication::applicationDirPath() +
        QStringLiteral("/module/dgl_my_plugin/python/read_stats.py");
    const QByteArray script = script_path.toUtf8();

    char out[4096] = {};
    char err[1024] = {};
    const int code = dglab_run_python(interpreter.c_str(), script.constData(), args.c_str(), 5000,
        out, sizeof(out), err, sizeof(err));
    if (code == DGLAB_PYTHON_OK) return std::string(out);
    if (code > 0) {
        // 脚本自己失败：退出码加 stderr，通常就是 traceback
        PLUGIN_LOG(this, PluginLogLevel::Warn, "脚本退出码 " << code << "：" << err);
        return std::string();
    }
    PLUGIN_LOG(this, PluginLogLevel::Warn, "运行脚本失败，错误码 " << code << "：" << err);
    return std::string();
}
```

`QCoreApplication::applicationDirPath()` 拼的是默认扫描目录 `<程序目录>/module/`；把 `app.module.path` 改过的话，按同一配置值去定位。

### 版本与 ABI 约定

两个版本号各管一段，不要混用：

| 版本号 | 定义位置 | 管什么 |
| --- | --- | --- |
| `PLUGIN_API_VERSION` | `include/plugin/plugin_export.h` | 插件接口集合（`IPlugin` / `IPluginHost` 的方法集），宿主加载时校验 |
| `DGLAB_SDK_VERSION` | `include/plugin/PathFinder.h` | SDK 的 C ABI（函数签名与结果结构），`dglab_sdk_version()` 返回编译期值 |

- **只有 `include/plugin/` 下的内容变化才递增 `PLUGIN_API_VERSION`**：`src/**` 与其它 include 子目录怎么改都不构成插件 ABI 变更。
- 结果结构由调用方分配、SDK 只写定长缓冲，跨 ABI 不传 STL 容器，也不要求两边分配器一致。
- 插件与宿主必须用同一工具链、同一 Qt 版本构建。
- SDK 不要静态链接进插件：静态链会得到第二份副本，宿主升级后插件仍在跑旧代码。
- 插件 zip 不打包 SDK，它随应用安装包分发、与可执行文件同级，见 [distribution.md](distribution.md)。

## 随插件分发的 Python 脚本

插件需要一点 Python 生态（解析文件、算数据、用现成库）时，不必把逻辑全塞进 C++，也不必自带解释器：把 `.py` 随插件发出去，运行时用 SDK 的 `dglab_run_python` 拉起宿主配置里那个解释器。

### 目录约定

| 位置 | 内容 |
| --- | --- |
| 源码 `module/<插件>/meta/python/<脚本>.py` | 随 `META_DIR` 一起被 `dgl_add_plugin` 复制到 `<构建目录>/module/<目标名>/python/` |
| 安装后 `<程序目录>/module/<目标名>/python/<脚本>.py` | 插件运行时的脚本目录，`<目标名>` 即动态库主干名（如 `dgl_gsi_plugin`） |
| 插件 zip 内 `<目标名>/python/<脚本>.py` | 打包脚本把侧车目录下的非图片文件递归打进 zip，`.py` 自然随行 |

插件 zip 只装你的东西：动态库、侧车与脚本。解释器与 `dglab_plugin_sdk` 都由应用提供，跟随应用安装目录，不随 zip 分发。

### 解释器来源

宿主配置 `python.path`（`config/main.json` 的 `python.path`，默认 `python`），插件通过 `IPluginHost::get_config_value("python.path", "python")` 取现值后原样传给 `dglab_run_python`：

```cpp
const std::string interpreter = host_->get_config_value("python.path", "python");
```

传空串时 SDK 回退到 `python`（走 `PATH`），所以「用户没配」与「用户配了系统 python」走同一条路径。不要把运行器再包一层 dll：SDK 已经在宿主进程里，多一层只多一份调用约定与超时逻辑，宿主升级 SDK 后还会与新 ABI 打架。

### 超时与输出缓冲

- `timeout_ms` 传 0 或负值取 SDK 默认值 30000 ms；解释器启动本身另有 5000 ms 上限，超时后 kill 并再等 2000 ms 回收，不留僵尸进程。
- stdout / stderr 各是一块调用方分配的缓冲，容量是**含结尾 NUL 的容量**；容量传 0 时对应指针可以传空指针（只关心退出码的场合）。
- 输出超过容量时按 `容量 - 1` 截断并补 NUL，返回 `DGLAB_PYTHON_ERROR_TRUNCATED`。这不是失败：脚本已经跑完，只是内容被裁掉，下次把缓冲放大或让脚本少打印。
- 两个缓冲互相独立，不要把同一块内存同时传给 stdout 与 stderr。

### 失败处理

返回码分三层，处置方式也不同：

| 返回码 | 含义 | 处置 |
| --- | --- | --- |
| `DGLAB_PYTHON_OK`（0） | 成功 | 读 stdout |
| 正数 | 子进程退出码 | 脚本自身失败，stderr 里通常是 traceback；记警告并跳过本次数据更新 |
| `DGLAB_PYTHON_ERROR_TIMEOUT` / `START` / `CRASH` | 超时 / 解释器起不来 / 崩溃 | 记警告，本周期不重试，等下一次调度再试，避免把调度器拖住 |
| `DGLAB_PYTHON_ERROR_ARGUMENT` / `INTERNAL` | 参数写错 / SDK 内部错误 | 记错误；参数类问题属于插件 bug，应在开发期修掉 |

实践上让脚本把结果打成一行 JSON 交给 `print`，插件侧解析失败就按「本次无数据」处理，比让脚本写临时文件再读更少踩坑。

## 相关文档

- [community_plugin_template/README.md](community_plugin_template/README.md)：最小可编译模板与侧车样例
- [../module/README.md](../module/README.md)：现有插件清单与命名约定
- [../include/plugin/IPlugin.h](../include/plugin/IPlugin.h)：接口定义
- [event-mapping.md](event-mapping.md)：侧车 `events` 声明的语义
- [features.md](features.md)：插件中心、加载状态与数值页签
- [build.md](build.md)：构建与测试
