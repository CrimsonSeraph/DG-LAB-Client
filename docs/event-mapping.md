# 事件与映射配置（events / user.json）

> 本文档描述插件的**事件声明**与用户的**映射配置**：插件在侧车 `plugin.json` 的 `events` 里声明「什么数值变化算一次事件」，用户在 `user.json` 的 `app.plugins.<模块名>` 里决定「这次事件输出到哪个通道、加多少」。数值注册与插件接口见 [`module/README.md`](../module/README.md)，起步模板见 [`docs/community_plugin_template/`](community_plugin_template/README.md)。

## 一、概念

事件、子选项、输出配置是三层嵌套关系，前者决定「何时触发」，后者决定「触发后输出什么」：

| 层 | 定义位置 | 作用 |
| --- | --- | --- |
| 事件 | `plugin.json` → `events[]` | 触发容器：监听该插件已注册的一个数值，按比较规则判定是否触发 |
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

buckshot 的 HP 减少（插件注册的数值 id 为 `hp`，每次掉血至少 1 点触发）：

```json
{
    "id": "hurt",
    "label": "受伤",
    "trigger": { "value": "hp", "compare": "decrease", "amount": 1 },
    "options": [{ "id": "light", "label": "轻伤" }, { "id": "heavy", "label": "重伤" }]
}
```

## 四、user.json 的映射结构

映射写在 `user.json` 的 `app.plugins.<模块名>` 下，键为插件 `name()` 返回的模块标识（如 `buckshot`）。

无子选项的事件（`options` 为空数组）在映射里同样占一个子选项条目，id 固定为 `default`：`events.<事件id>.options.default`，字段与普通子选项完全相同。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `channel` | string | 模块级默认输出通道：`A` / `B` / `AB`，缺省 `AB`；子选项 `channel` 为空串时按它展开，见「默认通道与跟随默认」 |
| `channels.A` / `channels.B` | object | 每个输出通道一份配置 |
| `channels.<通道>.limit` | number | 通道上限 |
| `channels.<通道>.safety` | number | 安全上限，输出强度的硬钳制 |
| `events.<事件id>.enabled` | bool | 该事件是否参与输出 |
| `events.<事件id>.wave` | string | 事件波形名，空串表示不附加波形 |
| `events.<事件id>.merge` | string | 该事件多路结果的合并规则：`multiply` / `max` / `min` / `weighted` |
| `events.<事件id>.weight` | number | 合并权重，`merge=weighted` 时使用 |
| `events.<事件id>.frames` | number | 事件激活窗口帧数（1 帧 = 100ms），默认 10（1 秒）；`0` 视为 1 帧，超过 600 截断为 600 |
| `events.<事件id>.options.<子选项id>` | object | 该子选项的输出配置 |
| `option.channel` | string | 输出通道：`A` / `B`；空串表示跟随模块默认通道 `channel` |
| `option.operation` | string | 操作：`increase` / `set` / `decrease` / `ramp_up` / `ramp_down` |
| `option.mode` | string | 取值模式：`value` / `percent` |
| `option.value` | number | `mode=value` 时使用的数值 |
| `option.percent` | number | `mode=percent` 时使用的百分比 |
| `option.step` | number | 每帧步长（帧固定 100ms），`ramp_up` / `ramp_down` 使用 |
| `option.edited_at` | number | 毫秒时间戳，写配置时刷新；「设为」互斥按它取最后编辑的一条 |
| `output.wave` | string | 输出端附加波形名，空串表示不附加 |
| `output.merge` | string | 输出端的合并规则，取值同 `merge`，默认 `multiply` |

```json
{
    "channel": "AB",
    "channels": { "A": { "limit": 200, "safety": 150 }, "B": { "limit": 200, "safety": 150 } },
    "events": {
        "gift": {
            "enabled": true,
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
```

### 默认通道与「跟随默认」

模块级 `channel` 决定子选项没有显式指定通道时的落点：

- `A`：只落到 A 通道；`B`：只落到 B 通道；`AB`（缺省）：A 与 B 各一份，两个通道各自独立累计，各自受自己的 `channels.<通道>.limit` 与 `channels.<通道>.safety` 约束。
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

### merge

| 取值 | 行为 |
| --- | --- |
| `multiply` | 各路结果相乘 |
| `max` | 取各路结果的最大值 |
| `min` | 取各路结果的最小值 |
| `weighted` | 按 `weight` 加权合并 |

`weighted` 的计算为 `Σ(weight_i × v_i) / Σ(weight_i)`；`weight` 缺省 1.0；`Σweight = 0` 时退化为算术平均。

### 强度合成

最终强度 = `Clamp(通道上限 × 百分比或叠加结果, 安全上限)`：通道上限取 `channels.<通道>.limit`，百分比取 `option.percent`，叠加结果由 `merge` 得出，最后按 `channels.<通道>.safety` 钳制，下限 0，输出整数。

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
