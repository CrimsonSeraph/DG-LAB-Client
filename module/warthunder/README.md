# 战争雷霆遥测插件（warthunder）

轮询本机 `127.0.0.1:8111` 的空战遥测接口，把速度、高度、姿态、动力与构型等遥测写入 `warthunder` 模块数值，供规则引擎引用。

## 功能

- 按配置周期向 `<endpoint>/state` 发起 HTTP GET，解析返回的 JSON 对象，当帧出现的数值一次性写入模块。
- 键名比较大小写不敏感且忽略空格：`IAS, km/h` 与 `ias,km/h` 视为同一个键。
- 值可以是数字、数字字符串或布尔（`false` → 0，`true` → 1），其它类型按缺失处理。
- 每个数值都有主键与后备键：主键取不到数值时尝试后备键，两者都取不到就跳过该数值，保留上一次的值，不写 0。
- `valid` 由接口返回的 `valid` 字段驱动；接口不可用或 JSON 解析失败时直接置 0。
- 接口不可用、解析失败只写限流后的调试日志（1 秒内不重复），不输出错误/警告级日志，不弹通知。

## 配置项

| 配置键 | 默认值 | 说明 |
| --- | --- | --- |
| `app.module.warthunder.endpoint` | `http://127.0.0.1:8111` | 遥测接口根地址（末尾斜杠自动去除） |
| `app.module.warthunder.period_ms` | `500` | 轮询周期毫秒数（小于 100 按非法值回退 500） |

## 数值

写入值 = `原值 × 定标倍数` 四舍五入后钳到该数值的注册范围。来源键列写作「主键 → 后备键」。

| 数值 ID | 显示名 | 单位与定标 | 来源键 |
| --- | --- | --- | --- |
| `speed` | 空速 | km/h，×1，范围 0–5000 | `IAS, km/h` → `TAS, km/h` |
| `tas` | 真空速 | km/h，×1，范围 0–5000 | `TAS, km/h` → `IAS, km/h` |
| `mach` | 马赫数 | ×1000，Mach 0.85 → 850，范围 0–5000（即 0–5 Mach） | `M` → `MACH` |
| `altitude` | 高度 | 米，×1，范围 -1000–30000 | `altitude, m` → `H, m` |
| `climb` | 垂直速度 | m/s ×10，向上为正，+12.5 m/s → 125，范围 -10000–10000 | `Vy, m/s` → `vertical speed, m/s` |
| `aoa` | 迎角 | deg ×10，-3.5° → -35，范围 -900–900（即 ±90°） | `AoA, deg` → `AoA` |
| `g_load` | 过载 | G ×10，6.5G → 65，范围 -500–500（即 ±50G） | `Ny` → `Ny, g` |
| `throttle` | 油门 | % ×1，范围 0–100 | `throttle 1, %` → `throttle, %` |
| `rpm` | 转速 | % ×1，范围 0–100 | `RPM 1, %` → `RPM 1` |
| `gear` | 起落架 | 0 收起 / 1 放下（取整后钳到 0–1，非 0 记为放下） | `gear, %` → `gear` |
| `flaps` | 襟翼 | % ×1，范围 0–100 | `flaps, %` → `flaps` |
| `valid` | 数据有效 | 1 本帧数据有效 / 0 接口不可用或解析失败 | `valid` |

- 数值在插件初始化时全量注册，之后只更新当帧出现的字段。上表的键在真实 `/state` 里若不存在，对应数值保持注册但一直不更新，规则与界面都不会因此出错。
- `rpm` 的后备键 `RPM 1` 在部分版本给的是发动机转数（数千转）而不是百分比，取整后会被钳到 100；接入时按实际接口核对，必要时改主键。
- 多引擎机型的油门/转速主键带引擎序号（`throttle 1, %`、`RPM 1, %`），只取 1 号引擎。

## 事件

侧车 `meta/plugin.json` 声明的事件，`trigger.value` 均引用上表注册的数值 ID：

| 事件 ID | 展示名 | 触发条件 | 子选项 |
| --- | --- | --- | --- |
| `altitude_low` | 高度下穿 1000 米 | `altitude` `cross_below` 1000 | `warn` 低空警告 / `danger` 危险低空 |
| `stall_warning` | 速度下穿 60 km/h | `speed` `cross_below` 60 | 无（空数组，映射按 `default` 配置） |
| `g_overload` | 过载增加 3.0G | `g_load` `increase` 30 | `gray_out` 灰视 / `blackout` 黑视 |
| `climb_surge` | 爬升加快 10 m/s | `climb` `increase` 100 | `climb_warn` 爬升提示 |
| `dive_surge` | 俯冲加快 10 m/s | `climb` `decrease` 100 | 无（空数组，映射按 `default` 配置） |
| `throttle_up` | 油门增加 10% | `throttle` `increase` 10 | `afterburner` 加力 |
| `mach_high` | 马赫数上穿 0.80 | `mach` `cross_above` 800 | `transonic` 跨音速 / `supersonic` 超音速 |
| `telemetry_update` | 遥测状态更新 | `valid` `any` | `lost` 数据中断 |

阈值取定标后的整数值：`g_load` 的 30 是 3.0G（×10），`climb` 的 100 是 10 m/s（×10），`mach` 的 800 是 Mach 0.80（×1000）。事件覆盖 `cross_below` / `cross_above` / `increase` / `decrease` / `any` 五种比较方式，语义见 [`docs/event-mapping.md`](../../docs/event-mapping.md)。

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
4. 想知道接口到底给哪些键，可直接在浏览器打开 `http://127.0.0.1:8111/state` 核对；本文数值表的后备键按常见写法选取，实际键名不符时该数值不更新。
