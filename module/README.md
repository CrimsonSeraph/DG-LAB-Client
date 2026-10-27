# 插件目录（module）

本目录存放 **动态库插件** 的源码。插件是符合 `IPlugin` 接口的动态库模块，由主程序（`ModuleManager`）在启动时扫描并动态加载/卸载，实现数值模块的热插拔扩展，无需重新编译主程序。

> 运行时扫描目录为 **可执行文件旁的 `module/` 目录**（不是本源码目录）；构建后插件 DLL 由 CMake `POST_BUILD` 自动复制到该目录。扫描路径可通过 `config/main.json` 的 `app.module.path` 更改。

## 目录结构

```text
module/
├── gsi/                # CS2 GSI 插件（完整功能示例：路径查找、配置生成、SteamID 归属区分）
├── warthunder/         # 战争雷霆遥测插件（轮询 127.0.0.1:8111 空战遥测）
├── buckshot/           # 恶魔轮盘桥接插件（WebSocket 服务端接收 JSON 事件）
└── README.md           # 本说明文件
```

社区插件模板（不参与构建）位于 [`docs/community_plugin_template/`](../docs/community_plugin_template/README.md)，复制到本目录下即可改成新插件。

## 插件清单

| 源码目录 | CMake 目标名 | 链接库 | 说明 |
| --- | --- | --- | --- |
| `module/gsi/` | `dgl_gsi_plugin` | `Qt::Core Qt::Network` | CS2 GSI 数据接收与数值输出 |
| `module/warthunder/` | `dgl_warthunder_plugin` | `Qt::Core Qt::Network` | 轮询本地空战遥测，输出速度/高度/有效性 |
| `module/buckshot/` | `dgl_buckshot_plugin` | `Qt::Core Qt::Network Qt::WebSockets` | WebSocket 服务端接收外部桥接事件 |

## 源码组织与命名约定

每个插件一个子目录，包含：

| 文件 | 说明 |
| --- | --- |
| `XxxPlugin.h` | 插件类声明（类名以 `Plugin` 结尾，继承 `IPlugin`） |
| `XxxPlugin.cpp` | 实现 + `extern "C"` 导出三件套（`get_plugin_api_version`/`create_plugin`/`destroy_plugin`） |
| `meta/plugin.json` | 展示元数据侧车（可选） |
| `README.md` | 插件自身说明（可选） |

- **插件名即模块标识**：`IPlugin::name()` 返回纯 ASCII 模块标识（如 `warthunder`），供规则引用与模块查询；展示名（可为中文）放侧车 `plugin.json` 的 `name` 字段，两者解耦。
- **CMake 目标名**：`dgl_<插件名>_plugin`；`dgl_add_plugin` 会设置 `PREFIX ""`，使动态库主干名等于目标名（Windows/Linux 都不带 `lib` 前缀）。
- **插件类不继承 `QObject`**、不使用 `Q_OBJECT`：插件构建不经过 moc，定时器/网络/WebSocket 用 lambda 连接（`QObject::connect(sender, signal, context, lambda)`，`context` 传对应的 `QObject` 指针）。

## 展示元数据侧车（plugin.json）

模块页的名称、作者、简介、头图、版本与标签来自侧车 `plugin.json`，源码侧固定放在 `module/<插件名>/meta/plugin.json`，部署后位于 `<exe>/module/<目标名>/plugin.json`（宿主按动态库主干名定位该目录）。字段全部可选，缺失或 JSON 损坏时回退到既有展示名称。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 插件标识，约定与 `name()` 一致 |
| `name` | string | 展示名 |
| `version` | string | 插件版本 |
| `author` | string | 作者 |
| `description` | string | 一句话简介 |
| `banner` | string | 头图文件名（只写文件名，文件与该插件的侧车同目录），留空则不显示；宿主按 `image://modulebanner/<插件文件名>/<banner>` 读取 |
| `tags` | string[] | 标签 |

侧车随插件在 `CMakeLists.txt` 中注册，`META_DIR` 必须指向 **侧车子目录**（不是插件源码目录）：

```cmake
dgl_add_plugin(dgl_warthunder_plugin
	SOURCES
		module/warthunder/WarThunderPlugin.h
		module/warthunder/WarThunderPlugin.cpp
	QT_LIBS Qt::Core Qt::Network
	META_DIR module/warthunder/meta
)
```

`dgl_add_plugin` 用 `copy_directory` 把 `META_DIR` 的内容整体复制到 `<exe>/module/<目标名>/`；若把插件源码目录当作 `META_DIR`，源码会被一起复制进部署目录。`META_DIR` 可写多个目录，未指定时只复制动态库。

头图是侧车同目录的普通图片文件：放进 `module/<插件名>/meta/` 就会随整目录一起部署到 `<exe>/module/<目标名>/`（`copy_directory` 复制的是目录内容，不只 `plugin.json`），侧车只写文件名。宿主用 `image://modulebanner/<插件文件名>/<banner>` 读取该文件，文件缺失、不是图片或 `banner` 里带路径分隔符时按无图处理（插件卡与详情页都保持空态）。

`module/gsi/meta/banner.png` 是本仓库自带的侧车头图样例（1200×400，程序绘制的反恐精英 2 头图），用于端到端验证头图渲染与整目录部署。

### 事件声明（events）

侧车的 `events` 数组声明「什么数值变化算一次事件」，事件下的 `options` 是输出变体（子选项）；用户在 `user.json` 的 `app.plugins.<模块名>` 里决定每个子选项输出到哪个通道、加多少。字段全部可选，缺省表示该插件不产生事件。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `events` | object[] | 事件声明数组 |
| `events[].id` | string | 事件标识，`user.json` 的 `events` 键引用它 |
| `events[].label` | string | 事件展示名 |
| `events[].trigger.value` | string | 引用本插件已注册的数值 id |
| `events[].trigger.compare` | string | `any` / `increase` / `decrease` / `cross_above` / `cross_below` |
| `events[].trigger.amount` | number | 比较阈值或最小变化量，`compare=any` 时忽略 |
| `events[].options` | object[] | 子选项数组，可为空数组（为空时映射按隐式子选项 `default` 读取） |
| `events[].options[].id` | string | 子选项标识，映射配置的 `options` 键引用它 |
| `events[].options[].label` | string | 子选项展示名 |

```json
{
    "events": [
        {
            "id": "hurt",
            "label": "受伤",
            "trigger": { "value": "hp", "compare": "decrease", "amount": 1 },
            "options": [{ "id": "light", "label": "轻伤" }]
        }
    ]
}
```

触发判定（`increase` / `decrease` 比较最小变化量，`cross_above` / `cross_below` 为严格穿越且首次写入不触发）、映射结构与输出合成规则见 [`docs/event-mapping.md`](../docs/event-mapping.md)。

## 插件快速上手

### 接口与宿主能力

- **插件接口**: `include/plugin/IPlugin.h` —— 生命周期（`initialize`/`uninitialize`/`cleanup`/`can_unload`）、自描述（`name`/`version`/`api_version`/`capabilities`/`dependencies`）、线程安全声明、`on_host_period_changed` 周期通知。
- **宿主上下文**: `include/plugin/PluginHost.h` —— `IPluginHost` 提供数值注册/写入、数据接收（`listen_data`/`register_data_handler`）、配置读写（`get_config_value`/`set_config_value`）、基础周期与用户通知（`notify`）。
- **导出宏**: `include/plugin/plugin_export.h` —— `PLUGIN_EXPORT`/`PLUGIN_API` 与 `PLUGIN_API_VERSION`。

### 最小示例（骨架）

```cpp
// MyPlugin.h
#include "PluginHost.h"

class MyPlugin : public IPlugin {
public:
    std::string name() const override { return "myplugin"; }
    std::string version() const override { return "0.1.0"; }
    PluginError initialize() override;   // 注册数值、启动数据来源
    void uninitialize() override;        // 注销数值与数据来源
};

// MyPlugin.cpp
PluginError MyPlugin::initialize() {
    if (!host_) {
        return PluginError::NotInitialized;
    }
    std::vector<ModuleValue> values;
    values.emplace_back("my_value", "我的数值", QueryPeriod::SECOND, "field");
    host_->register_module_values(name(), values, {"A", "B"});
    PLUGIN_LOG(this, PluginLogLevel::Info, "插件初始化完成");
    return PluginError::Ok;
}

extern "C" {
PLUGIN_EXPORT int get_plugin_api_version() { return PLUGIN_API_VERSION; }
PLUGIN_EXPORT IPlugin* create_plugin() { return new MyPlugin(); }
PLUGIN_EXPORT void destroy_plugin(IPlugin* plugin) { delete plugin; }
}
```

### CMake 注册

在根 `CMakeLists.txt` 的「插件」小节追加一块（Tab 缩进与既有块一致）：

```cmake
dgl_add_plugin(dgl_my_plugin
	SOURCES
		module/myplugin/MyPlugin.h
		module/myplugin/MyPlugin.cpp
	QT_LIBS Qt::Core Qt::Network
	META_DIR module/myplugin/meta
)
```

`dgl_add_plugin`（`cmake/Plugins.cmake`）负责：编译时定义 `PLUGIN_BUILD`、加入 `include/plugin` 与 `include/module` 包含路径、编入 `src/module/ModuleValue.cpp`、设置无前缀主干名、构建后复制动态库与侧车、登记安装规则。

### 扫描与加载

- 主程序扫描 `*.dll`/`*.so`/`*.dylib` 文件作为插件候选，**默认只扫描不加载**（模块页显示“暂未加载”，右侧“启用”按钮加载）。
- `app.module.scan_load=true` 时扫描即加载（调试用）。
- 加载流程: `QLibrary` → API 版本校验 → 依赖校验 → `create_plugin` → `attach_host` + `set_log_callback` → `initialize()`。
- 卸载流程: `can_unload()` → `uninitialize()` → `cleanup()` → `destroy_plugin()` → `QLibrary::unload`。

## 规范要求

- **日志**: 插件内部使用 `PLUGIN_LOG(this, level, ...)` 宏（不直接使用 `LOG_MODULE`），由宿主统一记录，类名/方法名自动为插件名称与函数名。
- **内存隔离**: 插件实例在插件内 `new`，宿主仅调用 `destroy_plugin`；禁止跨模块 `new`/`delete`、禁止静态全局变量。
- **错误处理**: 失败操作返回 `PluginError` 错误码，不抛异常。
- **ABI 兼容**: 必须与主程序使用同一工具链（MinGW g++ + Qt）构建。
- **API 版本**: 接口变更时递增 `PLUGIN_API_VERSION`。

## 详细说明

完整的插件接口说明、导出约定、宿主能力表格、构建示例与加载流程，请参阅根目录 [README.md](../README.md) 的“插件开发指南”小节；起步模板见 [`docs/community_plugin_template/`](../docs/community_plugin_template/README.md)。
