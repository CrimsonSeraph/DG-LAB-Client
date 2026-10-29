# 插件开发

插件把外部数据源接进宿主：宿主负责扫描、加载、调度与日志，插件只实现 `IPlugin` 接口。完整可编译的模板见 [community_plugin_template/README.md](community_plugin_template/README.md)，宿主侧的分层与组合根见 [architecture.md](architecture.md)。

## 插件系统

- **插件系统（IPlugin）** 提供插件化模块接口 `IPlugin`（纯虚基类）：生命周期（`initialize`/`uninitialize`/`cleanup`/`can_unload`）、自描述（名称、版本、API 版本、能力标志、依赖列表）、线程安全声明与错误码返回；统一跨平台导出宏 `PLUGIN_EXPORT`（Windows `_WIN32` / GCC/Clang 可见性），约定 `extern "C"` 的 `create_plugin`/`destroy_plugin`/`get_plugin_api_version` 工厂导出，主程序加载时校验 API 版本（`PLUGIN_API_VERSION`）。插件日志通过回调转发由宿主统一记录（不直接使用 `LOG_MODULE`，类名/方法名自动为插件自身名称与函数名）；内存隔离约定插件自行 `new`/`delete`（宿主仅调用 `destroy_plugin`）。`ModuleManager` 作为插件宿主：启动时扫描 `<程序目录>/module/`（路径由 `config/main.json` 的 `app.module.path` 配置，`app.module.scan_load` 控制"扫描即加载"），通过 `QLibrary` 动态加载、校验 API 版本与依赖、注入日志回调与宿主上下文（`IPluginHost`：数值注册/写入、共享 `DataListener`），并管理加载状态（暂未加载/已加载/加载失败）；卸载时执行 `can_unload` 检查 → `uninitialize` → `cleanup` → `destroy_plugin`。社区插件模板（`docs/community_plugin_template/`）演示完整的接口实现、导出与侧车样例；已注册的插件（`gsi`/`warthunder`/`buckshot`）构建后自动复制到 `<程序目录>/module/` 供扫描加载，展示元数据侧车随动态库放入 `module/<目标名>/plugin.json`。

## 接口与约定

插件化模块系统允许将数值模块（如 CS2 GSI）实现为独立的动态库，由主程序动态扫描/加载/卸载，无需重新编译主程序。

### 1. 目录与现有插件

- **源码目录**: `module/`（每个插件一个子目录，如 `module/gsi/`、`module/warthunder/`、`module/buckshot/`）。
- **运行时扫描目录**: 默认 `<程序目录>/module/`，构建后插件 DLL 自动复制至此；路径可由 `config/main.json` 的 `app.module.path` 更改。
- **现有插件**: `module/gsi/`（CS2 GSI 插件）、`module/warthunder/`（战争雷霆遥测插件）、`module/buckshot/`（恶魔轮盘桥接插件）；社区模板见 [`docs/community_plugin_template/`](community_plugin_template/README.md)。

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

### 7. 加载流程与状态

1. 主程序启动时扫描插件目录（`*.dll`/`*.so`/`*.dylib`），**默认只扫描不加载**，插件中心卡片显示「未启用」与「启用」按钮；`app.module.scan_load=true` 时扫描即加载（调试用）。
2. 加载流程: `QLibrary` 动态加载 → `get_plugin_api_version` 校验 → 依赖校验 → `create_plugin` → `attach_host` + `set_log_callback` → `initialize()`（插件在此注册数值与数据处理器）。
3. 卸载流程: `can_unload()` 检查 → `uninitialize()` → `cleanup()` → `destroy_plugin()` → `QLibrary::unload`。
4. 加载失败时记录原因，插件中心卡片显示「加载失败」。

### 8. GSI 插件数据归属规则（module/gsi/）

CS2 GSI 插件按 `player.steamid` 区分数据归属：首次收到有效数据时记录为本地玩家基准，之后比较——**一致为自身**（个人状态类数值正常更新：血量/护甲/金钱/闪光/烟雾/燃烧/回合击杀/爆头/总伤害/装备价值/总击杀/助攻/死亡/MVP 等），**不一致为队友**（个人数值不更新，仅团队/地图类数值更新：CT/T 得分、连续失利次数、炸弹状态、地图阶段）。字符串字段按固定集合映射为数字（如 `team` CT/T→3/2、`bomb.state`→1-4、`map.phase`→0-6），不固定的值放弃该字段。

## 相关文档

- [community_plugin_template/README.md](community_plugin_template/README.md)：最小可编译模板与侧车样例
- [../module/README.md](../module/README.md)：现有插件清单与命名约定
- [../include/plugin/IPlugin.h](../include/plugin/IPlugin.h)：接口定义
- [event-mapping.md](event-mapping.md)：侧车 `events` 声明的语义
- [features.md](features.md)：插件中心、加载状态与数值页签
- [build.md](build.md)：构建与测试
