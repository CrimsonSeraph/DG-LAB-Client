# 事件与映射配置（events / user.json）

> 本文档描述插件的**事件声明**与用户的**映射配置**：插件在侧车 `plugin.json` 的 `events` 里声明「什么数值变化算一次事件」，用户在 `user.json` 的 `app.plugins.<模块名>` 里决定「这次事件输出到哪个通道、加多少」；通道上限与安全上限属于通道，写在全局的 `app.channels.<通道>`。数值注册与插件接口见 [`module/README.md`](../module/README.md)，起步模板见 [`docs/community_plugin_template/`](community_plugin_template/README.md)。

## 一、概念

事件、子选项、输出配置是三层嵌套关系，前者决定「何时触发」，后者决定「触发后输出什么」：

| 层 | 定义位置 | 作用 |
| --- | --- | --- |
| 事件 | `plugin.json` → `events[]`，或代码内置的设备声明（见本节末「内置设备声明来源」） | 触发容器：监听该模块已注册的一个数值，按比较规则判定是否触发 |
| 子选项 | `plugin.json` → `events[].options[]` | 事件内的独立输出变体，例如「礼物」下的「小心心」与「玫瑰」 |
| 输出配置 | `user.json` → `events.<事件id>.options.<子选项id>` | 单个子选项的通道、操作、模式、数值或百分比、步长 |

- 事件与子选项的 `id` 是 `user.json` 的键：改 `id` 会让既有映射失效，改 `label` 只影响界面文字。
- `options` 可以是空数组：此时事件没有子选项，触发后不再区分变体；映射里按隐式子选项 id `default` 配置与读取（`events.<事件id>.options.default`）。

## 二、plugin.json 的 events 声明

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `events` | object[] | 否 | 事件声明数组；缺省表示该插件不产生事件 |
| `events[].id` | string | 是 | 事件标识，同一插件内唯一，`user.json` 的 `events` 用它作键 |
| `events[].label` | string | 是 | 事件展示名（事件列表、迷你窗、触发流水） |
| `events[].trigger` | object | 是 | 触发条件 |
| `events[].trigger.value` | string | 是 | 引用该插件已注册的数值 id（`register_module_values` 里的 id，如 `hp`、`altitude`） |
| `events[].trigger.compare` | string | 是 | 比较方式：`any` / `increase` / `decrease` / `cross_above` / `cross_below`，见第三节 |
| `events[].trigger.amount` | number | 否 | 比较阈值或最小变化量；`compare=any` 时忽略 |
| `events[].options` | object[] | 是（可为空数组） | 子选项声明 |
| `events[].options[].id` | string | 是 | 子选项标识，同一事件内唯一，映射配置的 `options` 用它作键 |
| `events[].options[].label` | string | 是 | 子选项展示名 |

```json
{
    "events": [
        {
            "id": "gift",
            "label": "礼物",
            "trigger": { "value": "hp", "compare": "decrease", "amount": 1 },
            "options": [{ "id": "heart", "label": "小心心" }, { "id": "rose", "label": "玫瑰" }]
        }
    ]
}
```

### 内置设备声明来源

设备类模块没有插件侧车，事件声明直接写在代码里，与插件侧车声明并列为两个来源：

| 来源 | 位置 | 载入方式 |
| --- | --- | --- |
| 插件声明 | 侧车 `plugin.json` 的 `events` 数组 | `ModuleManager::get_plugins()` 的 `PluginInfo::events`（解析路径未改动） |
| 内置声明 | 代码常量（`DeviceValueBridge::builtin_events()` / `DeviceValueBridge::civet_builtin_events()`） | `EventRuntime::register_builtin_events(module_name, events)` |

- 扩展点在 `EventRuntime`：注册表以模块标识为键（`builtin_events_`），`reload()` 先载入插件声明、再补内置声明；同一模块标识已由插件声明事件时以插件为准，内置声明被忽略。
- 重复注册同一模块标识是整体替换（不是追加），`reload()` 先 `engine_.clear()` 再重建，因此不会出现重复声明或重复触发。
- 注册后若运行时已 `initialize()`，立即 `reload()` 生效；否则在首次载入时生效。传入空列表等于清除该模块的内置声明。
- 注销走 `EventRuntime::unregister_builtin_events(module_name)`：从注册表移除该模块标识、丢弃引擎里它的运行期状态（`engine_.remove_module()`），再 `reload()` 与注册表对齐；插件仍声明同名模块时只清声明来源、不动插件的运行期状态。输入设备断开时按「注销数值模块 → 注销内置声明 → 清运行期状态」的顺序调用，日志里每个实例一行。
- 映射与启用规则完全不变：内置声明同样按 `app.plugins.<模块>.events.<事件>` 读映射、同样由级联集合决定是否启用（未映射的事件不参与输出）。

本版内置声明来自输入设备实例：**每台爪印 / 灵猫各自一个模块标识**，等于它的实例数值模块名 `paw_<MAC 后四位>` / `civet_<MAC 后四位>`（如 `paw_430C`），数值模块与事件模块同名。两台同型号同时在线时各有一份声明，互不覆盖；MAC 后四位撞车时实例名追加 `_2` / `_3`（`BleDeviceManager` 记一条 WARN）。旧的单实例模块名 `paw` / `civet` 不再注册，只作为旧配置迁移的来源（见 [devices.md](devices.md#输入设备实例名与旧配置迁移)）。

`paw_<MAC 后四位>`（爪印无线按钮传感器 V1.1，每台实例一份）：

| 事件 id | 展示名 | 触发条件 | 说明 |
| --- | --- | --- | --- |
| `button_pressed` | 按键按下 | `button`，`compare=increase`，`amount=1` | 设备上报 5A（进入触发状态）时 `button` 由 0 变 1 |
| `button_released` | 按键抬起 | `button`，`compare=decrease`，`amount=1` | 设备上报 5B（取消触发）时 `button` 由 1 变 0 |

两个事件都没有子选项，映射里按隐式子选项 id `default` 配置；首次写入 `button=0`（驱动启动时）只登记前值，不触发。

`civet_<MAC 后四位>`（灵猫边缘控制传感器 V1，每台实例一份）：

| 事件 id | 展示名 | 触发条件 | 说明 |
| --- | --- | --- | --- |
| `pressure_rise` | 气压上升 | `pressure`，`compare=increase`，`amount=1` | 气压相对上一次落值上升达到 1 个 0.01 kPa 单位 |
| `pressure_drop` | 气压下降 | `pressure`，`compare=decrease`，`amount=1` | 气压相对上一次落值下降达到 1 个 0.01 kPa 单位 |

灵猫只声明上升 / 下降两个事件，**不为具体阈值造事件**：阈值由规则（`cross_above` / `cross_below` 或规则图条件）自行配置，事件只区分方向。`pressure` 的单位是 0.01 kPa（设备原始 int16，÷100 即 kPa），映射与规则里的数值口径一致；`battery` / `raw` 不是事件来源。气压每 100ms 上报一次，但只有变化达到 `app.devices.civet.pressure_epsilon`（缺省 10，即 0.1 kPa）时才写数值，因此落在阈值以内的抖动不会产生事件，也不以 10Hz 触发规则求值；驱动启动时写入 `pressure=0` 基线，首帧按同一阈值与基线比较。

## 三、trigger.compare 的取值

| 取值 | 触发条件 | `amount` 含义 |
| --- | --- | --- |
| `any` | 该数值每次更新都触发 | 忽略 |
| `increase` | 正向变化量的绝对值不小于 `amount` | 最小正向变化量，`amount ≤ 0` 按 1 处理 |
| `decrease` | 负向变化量的绝对值不小于 `amount` | 最小负向变化量，`amount ≤ 0` 按 1 处理 |
| `cross_above` | 本次值越过阈值且上一次值在阈值下方（严格穿越） | 阈值 |
| `cross_below` | 本次值越过阈值且上一次值在阈值上方（严格穿越） | 阈值 |

- `increase` / `decrease` 比较变化量的绝对值：`increase` 只看正向增量，`decrease` 只看负向增量；`amount ≤ 0` 时按 1 处理。
- `cross_above` / `cross_below` 是严格穿越：上一次值与本次值必须分处阈值两侧，等于阈值不触发；该数值首次写入（没有上一次值）也不触发。

war thunder 的高度下穿（插件注册的数值 id 为 `altitude`，单位米，从 1000 米上方穿到下方时触发）：

```json
{
    "id": "altitude_below_1000",
    "label": "高度下穿 1000 米",
    "trigger": { "value": "altitude", "compare": "cross_below", "amount": 1000 },
    "options": []
}
```

按数值减少触发（插件注册的数值 id 为 `hp`，每次减少至少 1 点触发）：

```json
{
    "id": "hurt",
    "label": "受伤",
    "trigger": { "value": "hp", "compare": "decrease", "amount": 1 },
    "options": [{ "id": "light", "label": "轻伤" }, { "id": "heavy", "label": "重伤" }]
}
```

## 四、user.json 的映射结构

映射写在 `user.json` 的 `app.plugins.<模块名>` 下，键为插件 `name()` 返回的模块标识（如 `gsi`）。

无子选项的事件（`options` 为空数组）在映射里同样占一个子选项条目，id 固定为 `default`：`events.<事件id>.options.default`，字段与普通子选项完全相同。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `channel` | string | 模块级默认输出通道：`A` / `B` / `AB`，缺省 `AB`；子选项 `channel` 为空串时按它展开，见「默认通道与跟随默认」。**仅为兼容读取，界面不再暴露**：插件详情已没有该下拉，新配置不再写这个键，读取逻辑保留只为兼容既有 `user.json` |
| `events.<事件id>.dataType` | string | 输出数据类型：`strength` / `percent` / `wave`，缺省 `strength`，非法值按 `strength` 处理，见「输出数据类型」 |
| `events.<事件id>.wave` | string | 事件波形名，空串表示不附加波形 |
| `events.<事件id>.merge` | string | 该事件多路结果的合并规则：`multiply` / `max` / `min` / `weighted` |
| `events.<事件id>.weight` | number | 合并权重，`merge=weighted` 时使用 |
| `events.<事件id>.frames` | number | 事件激活窗口帧数（1 帧 = 100ms），默认 10（1 秒）；`0` 视为 1 帧，超过 600 截断为 600 |
| `events.<事件id>.options.<子选项id>` | object | 该子选项的输出配置 |
| `option.channel` | string | 输出通道：`A` / `B`；空串表示跟随模块默认通道 `channel`（该模块级键已无界面入口，空串按兼容读取的模块值展开，缺省 `AB`） |
| `option.operation` | string | 操作：`increase` / `set` / `decrease` / `ramp_up` / `ramp_down` |
| `option.mode` | string | 取值模式：`value` / `percent` |
| `option.value` | number | `mode=value` 时使用的数值 |
| `option.percent` | number | `mode=percent` 时使用的百分比 |
| `option.step` | number | 每帧步长（帧固定 100ms），`ramp_up` / `ramp_down` 使用 |
| `option.edited_at` | number | 毫秒时间戳，写配置时刷新；「设为」互斥按它取最后编辑的一条 |
| `output.wave` | string | 输出端附加波形名，空串表示不附加 |
| `output.merge` | string | 输出端的合并规则，取值同 `merge`，默认 `multiply` |

事件没有 `enabled` 字段：事件是否参与输出只由级联集合（运行期启用集合）决定。写入事件映射时不写 `enabled`，读取时也不读它——手工写进 `user.json` 的 `enabled` 会被忽略。

通道上限不在插件下，而在全局的 `app.channels.<通道>`，详见「通道上限」：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `app.channels.<通道>.limit` | number | 通道上限，缺省 200 |
| `app.channels.<通道>.safety` | number | 安全上限，输出强度的硬钳制，缺省 150 |

`user.json` 的片段（插件映射在 `app.plugins.<模块名>` 下，通道配置在全局的 `app.channels`）：

```json
{
    "app": {
        "channels": { "A": { "limit": 200, "safety": 150 }, "B": { "limit": 200, "safety": 150 } },
        "plugins": {
            "gsi": {
                "channel": "AB",
                "events": {
                    "gift": {
                        "dataType": "strength",
                        "wave": "",
                        "merge": "multiply",
                        "weight": 1.0,
                        "options": {
                            "heart": { "channel": "A", "operation": "increase", "mode": "value", "value": 5, "percent": 0, "step": 1, "edited_at": 0 },
                            "rose": { "channel": "A", "operation": "set", "mode": "percent", "value": 0, "percent": 20, "step": 1, "edited_at": 0 }
                        }
                    }
                },
                "output": { "wave": "", "merge": "multiply" }
            }
        }
    }
}
```

### 默认通道与「跟随默认」

模块级 `channel` 决定子选项没有显式指定通道时的落点：

- `A`：只落到 A 通道；`B`：只落到 B 通道；`AB`（缺省）：A 与 B 各一份，两个通道各自独立累计，各自受全局 `app.channels.<通道>` 的 `limit` 与 `safety` 约束。
- 子选项 `option.channel` 留空串表示「跟随默认」：按模块 `channel` 展开，`AB` 时该子选项在两个通道各生效一次，触发流水记两条（`channel` 字段分别为 `A` 与 `B`）。
- 子选项显式写 `A` / `B` 时以子选项为准，与模块 `channel` 的取值无关；空串映射不会被改写为具体通道，仍是「跟随」。
- `channel` 取 `A` / `B` / `AB` 之外的值一律按 `AB` 处理。
- 「设为」互斥按实际生效的通道分别判定：一条跟随默认且默认 `AB` 的 `set`，要在 A、B 两个通道上各自与同通道的其它 `set` 竞争。

## 五、输出语义

### operation

| 取值 | 行为 |
| --- | --- |
| `increase` | 在当前强度上增加，增量取自 `mode` 选中的字段 |
| `set` | 直接把强度设为给定值，受第六节互斥约束 |
| `decrease` | 在当前强度上减少，减量取自 `mode` 选中的字段 |
| `ramp_up` | 每个 100ms 帧对累加器增加 `step`，直到到达目标值，仅 `mode=value` |
| `ramp_down` | 每个 100ms 帧对累加器减少 `step`，直到到达目标值，仅 `mode=value` |

`ramp_up` / `ramp_down` 的帧固定 100ms；ramp 的目标值是该子选项按 `mode` 算出的值，每帧对累加器 ±`step`，到达目标即结束该 ramp 并归零。单个 ramp 激活最多 100 帧（10 秒）作保护，超时同样结束并归零。

事件触发后进入激活窗口（`events.<事件>.frames` 帧，1 帧 = 100ms）：窗口内累加器保持；窗口到期时该事件在通道上的基底与累加器（含未完成 ramp）整体归零；窗口内再次触发只重置窗口，不清零累加器。

窗口剩余帧数按 `events.<事件>.frames` 逐帧递减；但该事件任一通道上仍有未完成的 ramp（未到达目标且未触发 100 帧保护）时，本帧不递减窗口，ramp 结束后窗口从剩余值继续递减，到期照常归零。窗口以事件为单位，两通道共用同一份剩余帧数。

### mode

| 取值 | 生效字段 |
| --- | --- |
| `value` | `value` 生效，`percent` 忽略 |
| `percent` | `percent` 生效，`value` 忽略 |

### dataType

事件级字段，决定触发数值以什么形态折算成输出。缺省 `strength`，读到的值非法时同样按 `strength` 处理。

| 取值 | 语义 | 与百分比映射的关系 |
| --- | --- | --- |
| `strength` | 强度：数值直接可用的强度值，落到该通道时不再折算 | 与 `option.mode=value` 同形：数值按通道上限归一后参与合成 |
| `percent` | 百分比：数值先按该数值的 `min` / `max` 映射到 0~100，再按通道上限折算成强度 | 与 `option.mode=percent` 对应：折算后强度 = 百分比 ÷ 100 × 通道上限 |
| `wave` | 波形：数值绑定的波形帧序列参与叠加，逐帧按下文「波形包络」乘进结果，同时把波形帧下发到设备播放一次 | 波形只改变每帧的包络系数，百分比折算规则不变 |

- `percent` 的 0~100 映射区间取该事件触发数值的 `min` / `max`（数值未注册 `min` / `max` 时，`min` 按 0、`max` 按通道上限处理）。
- 三种取值只改变数值的解释方式，不改变触发判定、「设为」互斥与激活窗口的规则。
- 当前后端只做落盘与只读暴露（`EventBridge.events[].events[].dataType`）：强度合成仍按 `option.mode` 折算、按 `wave` 绑定波形包络，按 `dataType` 折算的接线随界面批次落地。

### merge

| 取值 | 行为 |
| --- | --- |
| `multiply` | 各路结果相乘 |
| `max` | 取各路结果的最大值 |
| `min` | 取各路结果的最小值 |
| `weighted` | 按 `weight` 加权合并 |

`weighted` 的计算为 `Σ(weight_i × v_i) / Σ(weight_i)`；`weight` 缺省 1.0；`Σweight = 0` 时退化为算术平均。

### 强度合成

最终强度 = `Clamp(通道上限 × 百分比或叠加结果, 安全上限)`：通道上限取 `app.channels.<通道>.limit`，百分比取 `option.percent`，叠加结果由 `merge` 得出，最后按 `app.channels.<通道>.safety` 钳制，下限 0，输出整数。

波形包络（每帧贡献）：

- 绑定了波形的事件，每帧贡献 = 该波形当前帧 4 个强度字节的均值 ÷ 100，得到 0~1 的包络。一帧为 16 进制 8 字节：前 4 字节是 4 个 25ms 子单元的频率（10~240），后 4 字节是对应强度（0~100），帧长 100ms，见 [`include/README.md`](../include/README.md) 的「V3 波形帧」。
- 未绑定波形的事件，其帧贡献恒为 1（不衰减）。
- 附加波形（`output.wave`）同样按上述包络计算，并与合并结果逐帧相乘。

### 合成顺序

合成按 事件内 → 通道内 → 输出端 → 终值 的顺序固定执行：

1. 事件内：同通道最多一条 `set` 作为基底，叠加 `increase` 之和、减去 `decrease` 之和、加上 ramp 累加器；先把结果按通道上限钳到 `[0, limit]`，再乘该事件的波形包络（未绑定波形时包络为 1）。
2. 通道内多事件：该通道上参与合成的事件按事件 id 字典序排序，第一个事件的归一值作初值；之后逐个折叠 `acc = merge(rule_i, [acc, v_i], [1.0, weight_i])`，`rule_i` 与 `weight_i` 取当前事件自身的字段，`v_i` 为该事件的归一值（事件内结果 ÷ `limit`）。折叠得到 0~1 的比例后再乘通道上限；`multiply` / `max` / `min` 的折叠与顺序无关，只有同一通道上混用不同规则时顺序才影响结果。
3. 输出端附加波形：与合成结果逐帧相乘，规则取 `output.merge`（默认 `multiply`）。
4. 终值：`Clamp(结果, 0, min(limit, safety))`，输出整数。

### 事件输出如何到达设备

事件引擎每帧算出的 A/B 强度（上一小节的终值）经 `EventRuntime::channelsChanged` 交给 `DeviceController::set_event_output()`；设备下发路径同时保存规则命令的最近值，两者按通道合并后下发：

- 合并：同一通道上取「事件输出」与「规则命令最近值」的较大者。规则命令只有「设为」（`mode=2`，含规则图通道节点与首页通道强度）参与合并；递减 / 递增 / 连减 / 连增是相对量，无法折算成绝对强度，仍按原样直发。
- 钳制：合并结果按全局通道配置的 `min(limit, safety)`（`DeviceController::channel_limit()`）钳到 `[0, 上限]`。
- 去重：最终强度与上一次下发值相同时不重复下发；帧间隔 100ms，B0 指令本身也每 100ms 由 `CoyoteBleController` 写入，上层不逐帧刷命令。
- 无输出目标（蓝牙未连接、没有已配对的 APP、没有 V4 被控方）时不触达下发链路，也不产生日志；目标在强度变化后恢复时按当前强度重发，强度整段不变期间重新接入则等下一次变化才下发。
- `dataType` 为 `strength` 与 `percent` 时下发走同一条路径：`percent` 已在引擎内按通道上限折算成强度，上层只处理最终强度。
- `dataType=wave` 除把绑定波形的每帧包络（见「强度合成」）乘进强度外，还把波形帧下发到设备播放一次，见下一小节「事件波形下发」。

### 事件波形下发（dataType=wave）

`dataType=wave` 且配置了 `events.<事件>.wave` 时，触发会把这个波形下发到设备播放一次；帧按发送协议 `app.wave.protocol` 取（`v2` 取 `frames_v2`、`v3` 取 `frames_v3`），协议与缺帧规则见 [wave-data.md](wave-data.md)。

| 环节 | 行为 |
| --- | --- |
| 触发 | 事件命中后，该事件有输出项的通道切换到该波形并从第一帧播放一次（帧长 100ms，12 帧的「呼吸」播放 1.2 秒）；波形包络仍按「强度合成」乘进该事件的强度 |
| 去重 | 同一波形播放期间再次触发不重启播放，只把剩余帧数续满；换成另一个波形触发则立即替换并从头播放 |
| 播完 | 播放进度按设备实际写入的 B0 帧数推进，写满该波形的帧数即收口 |
| 恢复 | 事件播放期间到达的规则波形命令优先恢复；否则恢复接管前正在播放的规则波形（按剩余时长重新下发，帧位置从第一帧起，设备侧帧位置不可回读）；两者都没有时静音该通道 |
| 冲突 | 事件波形优先：规则侧同通道正在播放的波形被临时接管，事件播完恢复；事件播放期间到达的规则波形命令只登记不下发 |
| 缺帧 | 当前协议下没有帧（含波形名不存在）时不发送、也不改变通道状态，记 `LOG_WARN` 并经 `DeviceController::lastError` / `errorOccurred` 走既有 Toast 提示；同一波形的重复拒绝只提示一次 |
| 通道关闭 / 设备断开 | 正在播放的事件波形立即中止并清空状态（不恢复接管前波形），重连后从下一次触发重新开始 |
| 无输出目标 | 蓝牙未连接、没有已配对的 APP、没有 V4 被控方时整条链路不触达，也不产生日志 |

状态机只有「空闲 → 事件波形播放 → 恢复」三态，转换条件即上表各行：空闲接到事件波形进入播放；播放中接到同一波形只续期、接到新波形则替换；写满帧数收口回恢复；输出目标不可用从中止回到空闲。状态与去重逻辑是 `include/event/EventMath.h` 的纯函数 `advance_event_wave()`，测试目标 `event_wave_test`。

### 通道上限

通道上限与安全上限属于通道而不是插件：A、B 各一个唯一值，所有插件共用，插件停用或换成另一个插件都不影响它。

- 存储位置：`user.json` 的 `app.channels.<通道>.{limit, safety}`（缺省 `limit=200`、`safety=150`）。
- 它是**通道级回退层**：默认设备若在 `app.devices.<设备 MAC>.{limit_a, safety_a, limit_b, safety_b}` 配了设备特有值，就按设备值执行；该键缺失或被删除时回退到这里。整条回退链只有一处实现（`DeviceController::limits_for(mac, channel)`）；这一层的界面入口是设备管理页的「通道级回退值」卡（按设备的上下限在相邻一张卡里），文案明确写作「设备未单独设置时使用」。
- 事件引擎在载入模块时按通道读取同一份全局值，同一通道上的多个模块拿到的是同一对上限。
- 主页与状态栏展示的上限是 `min(limit, safety)`，与详情页写入的是同一份配置。
- 迁移：旧版本把上限写在 `app.plugins.<模块>.channels.<通道>`。读取时若全局键 `app.channels.<通道>` 缺失，就按模块名字典序取旧键中首个存在的值写回全局键并返回；写入之后只读全局键，旧键不再参与，也不再被清理或改写。

## 六、「设为」互斥

`operation=set` 在同一通道内同时只允许一条生效。检测是纯计算（只读内存中的配置，不落盘），只有清理才改写 `user.json`：

- 检测：比较该通道上所有 `set` 配置的 `edited_at`，保留时间戳最大（最后编辑）的一条，其余判为待清理。
- 清理：从 `user.json` 删除待清理子选项的映射条目，该子选项随即回落默认值 `operation=increase`、`value=0`、`percent=0`、`step=1`、`edited_at=0`。
- 提示：清理返回被清理项的「事件 / 子选项」标识，供 UI 提示用户。
- 子选项没有 `enabled` 字段：清理以删除映射条目表达，契约不新增子选项级开关。

## 七、触发流水

每次触发记录一条流水，供迷你窗展示：

| 记录字段 | 说明 |
| --- | --- |
| 时间 | 触发时刻 |
| 模块 | 插件模块标识（`IPlugin::name()`） |
| 事件 | 事件的 `label` |
| 子选项 | 子选项的 `label`；无子选项时留空 |
| 通道 | 实际输出的通道 `A` / `B` |
| 操作 | `operation` 取值 |
| 变化量 | 本次输出引起的强度变化量 |
