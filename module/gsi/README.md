# 反恐精英 2 插件（gsi）

通过 CS2 官方 Game State Integration 读取对局数据：插件用 PathFinder 定位游戏目录、生成 `gamestate_integration_dglab.cfg`、监听本机端口接收游戏推送，把玩家、队伍、地图与炸弹状态写成 `CS2 GSI 模块` 的数值，供规则引擎与事件引擎引用。

侧车 `meta/plugin.json` 声明 31 个事件，全部引用本插件真实注册的数值 ID。

## 功能

- 插件名 `CS2 GSI 模块`（沿用旧名称，规则引用不受影响），展示名「反恐精英 2」来自侧车 `name` 字段。
- 初始化时一次性注册 25 项数值，之后每次收到 GSI 推送只更新当帧出现的字段；字段缺失时不写入，保留上一次的值，不写 0。
- 自身/队友归属判定：首次收到带 `player.steamid` 的数据时把它记为本地玩家基准，之后一致为自身、不一致视为正在观战队友。自身数据更新个人数值，队友数据只更新团队/地图类数值。
- 字符串字段用固定映射表转成整数，未列出的取值直接丢弃（不写 0、不猜）。映射表见下文「非数值映射」。
- GSI 配置的 `throttle` 跟随模块最小查询周期；周期变化后重写配置，若 `cs2.exe` 正在运行则弹通知提示重启游戏（GSI 配置只在游戏启动时加载）。

## 配置项

插件通过宿主配置接口读写 `user.json` 的 `app.gsi` 分组：

| 配置键 | 说明 |
| --- | --- |
| `app.gsi.cs_dir` | 已定位的 CS 游戏目录；目录失效时下次启动重新查找 |
| `app.gsi.config_path` | 生成/复用的 GSI 配置文件绝对路径 |
| `app.gsi.port` | 监听端口（20000–60000 随机，端口被占用时重新随机并重写配置，最多重试 5 次） |

## 数值

范围即注册时登记的上下限（`register_module_values`），写入值会被钳制到该区间。「归属」列说明该数值在观战队友时是否仍更新。

| 数值 ID | 显示名 | 来源字段 | 范围 | 归属 |
| --- | --- | --- | --- | --- |
| `health` | 当前血量 | `player.state.health` | 0–100 | 自身 |
| `armor` | 当前护甲 | `player.state.armor` | 0–100 | 自身 |
| `money` | 金钱 | `player.state.money` | 0–16000 | 自身 |
| `team_num` | 队伍编号 | `player.team`（映射 `CT`→3、`T`→2） | 2–3 | 自身 |
| `has_helmet` | 是否有头盔 | `player.state.helmet`（布尔→1/0） | 0–1 | 自身 |
| `has_defuser` | 是否有拆弹器 | `player.state.defusekit`（布尔→1/0） | 0–1 | 自身 |
| `flashed` | 闪光致盲程度 | `player.state.flashed` | 0–255 | 自身 |
| `smoked` | 烟雾遮蔽程度 | `player.state.smoked` | 0–255 | 自身 |
| `burning` | 燃烧灼烧程度 | `player.state.burning` | 0–255 | 自身 |
| `round_kills` | 当前回合击杀数 | `player.state.round_kills` | 0–255 | 自身 |
| `round_killhs` | 当前回合爆头击杀数 | `player.state.round_killhs` | 0–255 | 自身 |
| `round_totaldmg` | 当前回合总伤害 | `player.state.round_totaldmg` | 0–10000 | 自身 |
| `equip_value` | 装备总价值 | `player.state.equip_value` | 0–65535 | 自身 |
| `kills` | 总击杀数 | `player.match_stats.kills` | 0–99999 | 自身 |
| `assists` | 总助攻数 | `player.match_stats.assists` | 0–99999 | 自身 |
| `deaths` | 总死亡数 | `player.match_stats.deaths` | 0–99999 | 自身 |
| `mvps` | MVP 次数 | `player.match_stats.mvps` | 0–99999 | 自身 |
| `ct_score` | CT 队伍得分 | `map.team_ct.score` | 0–999 | 全局 |
| `t_score` | T 队伍得分 | `map.team_t.score` | 0–999 | 全局 |
| `ct_consecutive_round_losses` | CT 连续失利次数 | `map.team_ct.consecutive_round_losses` | 0–99 | 全局 |
| `t_consecutive_round_losses` | T 连续失利次数 | `map.team_t.consecutive_round_losses` | 0–99 | 全局 |
| `bomb_state` | 炸弹状态 | `bomb.state`（映射见下） | 0–4 | 全局 |
| `map_phase` | 地图阶段 | `map.phase`（映射见下） | 0–6 | 全局 |
| `round_phase` | 回合阶段 | `round.phase`（映射见下） | 0–2 | 全局（本次补齐） |
| `activity` | 游戏活动状态 | `player.activity`（映射见下） | 0–2 | 全局（本次补齐） |

## 非数值映射

CS2 的 GSI 里有若干字符串/布尔字段，规则与事件只能吃整数，因此统一按下表转换。原则与旧实现一致：只映射文档给出的固定取值集合，集合外的取值不写入。

| 来源字段 | 映射 | 目标数值 | 说明 |
| --- | --- | --- | --- |
| `player.team` | `CT`→3、`T`→2 | `team_num` | 字段本身是数字时直接使用，不做映射 |
| `player.state.helmet` / `player.state.defusekit` | 布尔 `false`→0、`true`→1 | `has_helmet` / `has_defuser` | 布尔即 0/1，无需枚举表 |
| `bomb.state` | `planted`→1、`exploding`→2、`exploded`→3、`defused`→4 | `bomb_state` | 0 未被占用（用来装「未安放」的取值不在旧映射内），因此事件用 `increase` 表达状态推进 |
| `map.phase` | `warmup`→0、`calibration`→1、`teamchange`→2、`firsthalf`→3、`halftime`→4、`secondhalf`→5、`gameover`→6 | `map_phase` | 与注册范围 0–6 一一对应 |
| `round.phase` | `freezetime`→0、`live`→1、`over`→2 | `round_phase` | 本次补齐；此前该字段被整体忽略 |
| `player.activity` | `menu`→0、`playing`→1、`textinput`→2 | `activity` | 本次补齐；此前该字段被整体忽略 |

映射在代码里以 `std::map<QString, int>` 常量的形式与提取逻辑写在一起（`CS2GsiPlugin::update_self_values` / `CS2GsiPlugin::update_team_values`），表与注释同源，改表时两处一起改。

## 事件

侧车 `meta/plugin.json` 按 [`docs/event-mapping.md`](../../docs/event-mapping.md) 的契约声明事件，`trigger.value` 全部命中上表注册的数值 ID，25 个数值每一个至少被一个事件引用。

| 事件 ID | 展示名 | 触发条件 | 子选项 |
| --- | --- | --- | --- |
| `health_drop` | 血量下降 | `health` `decrease` 20 | `light` 轻伤 / `heavy` 重伤 |
| `health_critical` | 血量濒死 | `health` `cross_below` 25 | `near_death` 濒死 |
| `armor_break` | 护甲耗尽 | `armor` `cross_below` 1 | `cracked` 护甲击破 |
| `helmet_pickup` | 拾取头盔 | `has_helmet` `increase` 1 | `helmet` 头盔 |
| `defuser_pickup` | 拾取拆弹器 | `has_defuser` `increase` 1 | `defuser` 拆弹器 |
| `flash_blind` | 被闪光致盲 | `flashed` `cross_above` 128 | `blind` 致盲 |
| `flash_recover` | 闪光消退 | `flashed` `cross_below` 64 | 无（空数组，映射按隐式子选项 `default` 配置） |
| `smoked_start` | 进入烟雾 | `smoked` `cross_above` 1 | `smoked` 烟雾中 |
| `smoke_clear` | 脱离烟雾 | `smoked` `cross_below` 64 | 无（空数组，映射按隐式子选项 `default` 配置） |
| `burning_start` | 开始燃烧 | `burning` `cross_above` 1 | `burning` 燃烧中 |
| `kill` | 回合击杀 | `round_kills` `increase` 1 | `single` 单杀 / `multi` 多杀 |
| `headshot` | 爆头击杀 | `round_killhs` `increase` 1 | `headshot` 爆头 |
| `damage_dealt` | 造成伤害 | `round_totaldmg` `increase` 50 | 无（空数组，映射按隐式子选项 `default` 配置） |
| `death` | 自身阵亡 | `deaths` `increase` 1 | `death` 阵亡 |
| `mvp` | 获得 MVP | `mvps` `increase` 1 | `mvp` MVP |
| `assist` | 助攻 | `assists` `increase` 1 | `assist` 助攻 |
| `kill_total` | 总击杀增加 | `kills` `increase` 1 | `kill` 击杀 |
| `ct_loss_streak` | CT 连败增加 | `ct_consecutive_round_losses` `increase` 1 | `ct_loss` CT 连续失利 |
| `t_loss_streak` | T 连败增加 | `t_consecutive_round_losses` `increase` 1 | `t_loss` T 连续失利 |
| `money_surge` | 金钱增加 | `money` `increase` 1000 | 无（空数组，映射按隐式子选项 `default` 配置） |
| `money_spend` | 金钱消耗 | `money` `decrease` 500 | 无（空数组，映射按隐式子选项 `default` 配置） |
| `equip_loss` | 装备价值下降 | `equip_value` `decrease` 1000 | 无（空数组，映射按隐式子选项 `default` 配置） |
| `ct_score_up` | CT 得分 | `ct_score` `increase` 1 | `ct_round` CT 回合得分 |
| `t_score_up` | T 得分 | `t_score` `increase` 1 | `t_round` T 回合得分 |
| `bomb_state_advance` | 炸弹状态推进 | `bomb_state` `increase` 1 | `planted` 安放 / `exploding` 引爆中 / `defused` 拆除 / `exploded` 爆炸 |
| `bomb_exploded` | 炸弹爆炸 | `bomb_state` `cross_above` 2 | `exploded` 爆炸 |
| `bomb_defused` | 炸弹拆除 | `bomb_state` `cross_above` 3 | `defused` 拆除 |
| `round_phase_change` | 回合阶段变化 | `round_phase` `any` | `freezetime` 回合准备 / `live` 回合进行 / `over` 回合结束 |
| `map_phase_change` | 地图阶段变化 | `map_phase` `any` | `warmup` 热身 / `firsthalf` 上半场 / `halftime` 中场 / `secondhalf` 下半场 / `gameover` 比赛结束 |
| `activity_change` | 游戏活动变化 | `activity` `any` | `menu` 主菜单 / `playing` 对局中 / `textinput` 输入中 |
| `team_switch` | 阵营变化 | `team_num` `any` | `ctside` CT 阵营 / `tside` T 阵营 |

触发判定按 `EventEngine` 的实现，与 `docs/event-mapping.md` 一致：

- `any`：数值发生变化即触发（同值重发不触发）。
- `increase` / `decrease`：变化量不小于 `amount` 才触发，`amount ≤ 0` 按 1 处理。
- `cross_above`：上一次值 **严格小于** `amount` 且本次值 **严格大于** `amount`；`cross_below` 反向。数值首次写入没有上一次值，不触发。

由此带来两个取舍，列在这里以免被当成 bug：

- `bomb_state` 的取值只有 1–4，没有 0。`cross_above 0` 要求上一次值小于 0，永远不会成立，所以「安放」这类事件用 `bomb_state` `increase` 1 表达状态推进（0→1 不存在，1→2、2→3、3→4 都会触发），而「爆炸」「拆除」用 `cross_above 2` / `cross_above 3` 精确落在 3 / 4 上。
- `flashed` / `smoked` / `burning` 是 0–255 的强度值，「进入」用 `cross_above` 小阈值、「退出」用 `cross_below` 中阈值表达区间判断，而不是判断等值——`amount` 只是阈值，不是区间宽度。

保留 `health_drop` / `armor_break` / `money_surge` 三个既有事件 ID 不变，已有 `user.json` 映射继续生效；新增事件是纯增量。

## 溯源

本插件的两代实现都在这份仓库里，下列结论可用 `git log` / `git show` 复核：

| 提交 | 内容 |
| --- | --- |
| `89cc329` | 首次引入 `CS2GSIModule`（`include/module/CS2GSIModule.h`、`src/module/CS2GSIModule.cpp`），默认数值 6 项：`health`、`armor`、`team_num`、`money`、`has_helmet`、`has_defuser` |
| `3be76c6` | 开始解析 GSI 推送并写入上表 6 项；`player.team` 的 `CT`→3、`T`→2 是旧实现里**唯一**的非数值映射 |
| `cac87bd` | 收包层换成 `DataListener`，数值集合不变 |
| `0093935` | 改造为动态库插件 `CS2GsiPlugin`：数值由 6 项扩到 23 项，并新增 `bomb.state`（1–4）与 `map.phase`（0–6）两条映射 |
| `4420e27` | 每个数值登记上下限（范围），即本 README 数值表的「范围」列 |
| `99c78cc` | 新增侧车 `meta/plugin.json`，首次声明事件——当时只有 3 个 |

差集（旧实现 vs 侧车声明）：

- 数值：旧实现 6 项全部保留，插件化时新增 17 项，本次再补 2 项（`round_phase`、`activity`）→ 旧数值无缺项。
- 非数值映射：旧实现只有 `team`，现插件有 `team`、`bomb.state`、`map.phase`，本次补 `round.phase`、`player.activity` → 旧映射无缺项。
- 事件：仓库历史里 CS2 事件从未超过 3 个（`git log --all -S"events" -- module/gsi` 只有 `99c78cc` 一次提交），所谓「事件变少」不是功能回归，而是侧车只覆盖了 23 个注册数值里的 3 个。本次把 25 个数值全部接入事件，事件数 3 → 31。

## 构建与部署

插件由根 `CMakeLists.txt` 的插件小节注册（`dgl_gsi_plugin`，链接 Qt Core/Network，`META_DIR module/gsi/meta`）。构建后动态库复制到 `<程序目录>/module/`，侧车整体复制到 `<程序目录>/module/dgl_gsi_plugin/plugin.json`。

## 使用

1. 启用本插件，首次运行会定位游戏目录并生成 `gamestate_integration_dglab.cfg`，端口写入 `user.json`。
2. 启动 CS2（配置只在启动时加载；若插件改过配置需重启游戏）。
3. 在模块页查看数值刷新；在事件映射页为需要的事件子选项绑定通道与输出。
