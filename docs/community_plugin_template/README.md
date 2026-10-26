# 社区插件模板

本目录是 DG-LAB-Client 动态库插件的起步模板，演示 `IPlugin` 接口实现、数值注册、宿主配置读取与 `extern "C"` 导出约定。模板本身不参与构建，复制出去改写成你自己的插件即可。

## 目录结构

```text
docs/community_plugin_template/
├── README.md            # 本说明
├── ExamplePlugin.h      # 插件类声明
├── ExamplePlugin.cpp    # 实现 + 导出三件套
└── meta/
    └── plugin.json      # 展示元数据侧车（字段示例）
```

## 快速开始

1. 复制整个目录到 `module/<你的插件名>/`，例如 `module/myplugin/`。
2. 重命名 `ExamplePlugin.h` / `ExamplePlugin.cpp` 为 `MyPlugin.h` / `MyPlugin.cpp`，类名改为 `MyPlugin`，`name()` 改成你的模块标识（如 `myplugin`）。
3. 改写 `initialize()` 里的数值列表与数据来源，`uninitialize()` 与 `initialize()` 保持对称。
4. 改写 `meta/plugin.json` 的展示字段。
5. 在根 `CMakeLists.txt` 的「插件」小节注册（见下文），重新构建。

## 插件名即模块标识

- `IPlugin::name()` 返回**模块标识**：纯 ASCII、小写，供规则引用（`{id:xxx}` 形式的数值选择）与模块查询使用。
- 侧车 `plugin.json` 的 `name` 字段是**展示名**，可以是中文，只用于模块页展示。
- 两者解耦：改展示名不影响规则引用；改模块标识会让既有规则失效。

## plugin.json 字段

侧车文件放在 `meta/plugin.json`，字段全部可选，缺失或 JSON 损坏时模块页回退到默认展示名。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 插件标识，约定与 `name()` 一致 |
| `name` | string | 展示名（模块页标题） |
| `version` | string | 插件版本，建议与 `version()` 一致 |
| `author` | string | 作者 |
| `description` | string | 一句话简介 |
| `banner` | string | 头图文件名，相对侧车目录；留空则不显示头图 |
| `tags` | string[] | 标签，模块页显示为徽标 |

## CMake 注册

根 `CMakeLists.txt` 的「插件」小节统一使用 `dgl_add_plugin`（定义在 `cmake/Plugins.cmake`）：

```cmake
dgl_add_plugin(dgl_my_plugin
	SOURCES
		module/myplugin/MyPlugin.h
		module/myplugin/MyPlugin.cpp
	QT_LIBS Qt::Core Qt::Network
	META_DIR module/myplugin/meta
)
```

- `SOURCES` 必填，插件源码与头文件一并列出。
- `QT_LIBS` 按需追加（`Qt::Core` 必选，HTTP 用 `Qt::Network`，WebSocket 用 `Qt::Network Qt::WebSockets`）。
- `META_DIR` 指向**放侧车文件的目录**，可写多个；未指定则只复制动态库。
- `dgl_add_plugin` 已把 `include/plugin` 与 `include/module` 加入包含路径，插件源码 `#include "PluginHost.h"` 即可（它带入 `IPlugin.h` 与 `ModuleValue.h`）。

## 构建与部署

1. 配置并构建工程（插件随主工程一起构建）。
2. 构建后 `dgl_add_plugin` 的 `POST_BUILD` 步骤把动态库复制到 `<构建目录>/module/`，并把 `META_DIR` 内容整体复制到 `<构建目录>/module/<目标名>/`。
3. 安装规则 `dgl_install_plugins()` 把同样的布局装到 `<安装目录>/module/` 与 `<安装目录>/module/<目标名>/`。
4. 启动主程序，在模块页点击「启用」加载插件。

## META_DIR 必须指向侧车子目录

`dgl_add_plugin` 用 `copy_directory` 把 `META_DIR` 的内容整体搬到 `<目标名>/` 下，因此：

- 正确：`META_DIR module/myplugin/meta` → 部署为 `<exe>/module/dgl_my_plugin/plugin.json`。
- 错误：`META_DIR module/myplugin` → 源码目录被整份复制进部署目录，源码混入安装包。

宿主按「动态库主干名」定位侧车：`<exe>/module/<动态库主干名>/plugin.json`。`dgl_add_plugin` 已把主干名固定为等于目标名（`PREFIX ""`，无 `lib` 前缀），所以目录名就是 `dgl_my_plugin`。

## 构建约束

- 插件类**不继承 `QObject`**、不使用 `Q_OBJECT`：插件构建不经过 moc，定时器/网络/WebSocket 一律用 lambda 表达式连接（`QObject::connect(sender, signal, context, lambda)`），`context` 用对应的 `QObject` 指针。
- 只使用 `include/plugin` 与 `include/module` 暴露的接口；跨模块 `new`/`delete` 属未定义行为。
- 失败返回 `PluginError` 错误码，不抛异常；日志统一走 `PLUGIN_LOG(this, 级别, 内容)`。

接口与宿主能力的完整说明见 [`module/README.md`](../../module/README.md)，工程级插件开发指南见根 [`README.md`](../../README.md) 的「插件开发指南」小节。
