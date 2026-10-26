# 战争雷霆遥测插件（warthunder）

轮询本机 `127.0.0.1:8111` 的空战遥测接口，把空速、高度与数据有效性写入 `warthunder` 模块数值，供规则引擎引用。

## 功能

- 按配置周期向 `<endpoint>/state` 发起 HTTP GET，解析返回的 JSON。
- 空速取大小写不敏感的 `IAS, km/h`，缺失时回退 `TAS, km/h`，取整为 km/h。
- 高度取大小写不敏感的 `altitude, m`，取整为米。
- `valid` 缺失按 0；接口不可用或解析失败时把 `valid` 置 0。
- 接口不可用、解析失败只写限流后的调试日志（1 秒内不重复），不输出错误/警告级日志，不弹通知。

## 配置项

| 配置键 | 默认值 | 说明 |
| --- | --- | --- |
| `app.module.warthunder.endpoint` | `http://127.0.0.1:8111` | 遥测接口根地址（末尾斜杠自动去除） |
| `app.module.warthunder.period_ms` | `500` | 轮询周期毫秒数（小于 100 按非法值回退 500） |

## 数值

| 数值 ID | 名称 | 说明 |
| --- | --- | --- |
| `speed` | 空速 | 空速取整（km/h），范围 0–5000 |
| `altitude` | 高度 | 高度取整（米），范围 -1000–30000 |
| `valid` | 数据有效 | 1 表示本帧有效，0 表示接口不可用或解析失败 |

模块名与插件 `name()` 一致，均为 `warthunder`；展示名「战争雷霆」来自侧车 `meta/plugin.json` 的 `name` 字段。

## 构建与部署

插件由根 `CMakeLists.txt` 的插件小节统一注册：

```cmake
dgl_add_plugin(dgl_warthunder_plugin
	SOURCES
		module/warthunder/WarThunderPlugin.h
		module/warthunder/WarThunderPlugin.cpp
	QT_LIBS Qt::Core Qt::Network
	META_DIR module/warthunder/meta
)
```

构建后动态库复制到 `<程序目录>/module/`，侧车目录 `meta/` 的内容整体复制到 `<程序目录>/module/dgl_warthunder_plugin/`，即展示元数据位于 `<程序目录>/module/dgl_warthunder_plugin/plugin.json`。

## 使用

1. 启动游戏并开启本地遥测端口（默认 8111，可在游戏内设置处查看）。
2. 在模块页启用本插件，数值随轮询刷新。
3. 游戏未启动时插件保持静默轮询，`valid` 为 0，不产生错误日志。
