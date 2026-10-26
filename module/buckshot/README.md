# 恶魔轮盘桥接插件（buckshot）

在本机端口启动 WebSocket 服务端，接收外部桥接程序推送的 JSON 事件，把生命值、本回合伤害与护盾写入 `buckshot` 模块数值。

## 功能

- 监听 `127.0.0.1:8137`（仅本机回环，端口可配置），接受多个客户端连接。
- 接收文本帧 `{"type": "hp", "value": 18}`，`type` 白名单为 `hp` / `damage` / `shield`。
- 命中白名单的事件写入同名数值；未知类型、非对象帧、缺少数值字段的帧一律忽略并记调试日志。
- 监听失败（端口被占用）时初始化返回 `PluginError::ResourceError`，并记一条警告日志。

## 配置项

| 配置键 | 默认值 | 说明 |
| --- | --- | --- |
| `app.module.buckshot.port` | `8137` | WebSocket 监听端口（非法值回退 8137） |

## 数值

| 数值 ID | 名称 | 说明 |
| --- | --- | --- |
| `hp` | 生命值 | `{"type": "hp", "value": N}` 写入 |
| `damage` | 本回合伤害 | `{"type": "damage", "value": N}` 写入 |
| `shield` | 护盾值 | `{"type": "shield", "value": N}` 写入 |

数值不设上下限，外部桥接上报的整数原样写入；`value` 为浮点时四舍五入取整。

## 事件协议

```json
{"type": "hp", "value": 18}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `type` | string | 数值 ID，取值 `hp` / `damage` / `shield` |
| `value` | number | 数值，必填；浮点按四舍五入取整 |

## 构建与部署

插件由根 `CMakeLists.txt` 的插件小节统一注册：

```cmake
dgl_add_plugin(dgl_buckshot_plugin
	SOURCES
		module/buckshot/BuckshotPlugin.h
		module/buckshot/BuckshotPlugin.cpp
	QT_LIBS Qt::Core Qt::Network Qt::WebSockets
	META_DIR module/buckshot/meta
)
```

构建后动态库复制到 `<程序目录>/module/`，侧车目录 `meta/` 的内容整体复制到 `<程序目录>/module/dgl_buckshot_plugin/`，即展示元数据位于 `<程序目录>/module/dgl_buckshot_plugin/plugin.json`。

## 使用

1. 在模块页启用本插件，服务端随初始化启动。
2. 外部桥接程序连接 `ws://127.0.0.1:8137`，按事件协议推送文本帧。
3. 卸载插件时全部客户端连接关闭，端口释放。
