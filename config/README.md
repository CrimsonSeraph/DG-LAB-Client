# 配置文件目录（config）

本目录存放 DG-LAB-Client 的全部系统配置内容。客户端启动时会自动加载本目录下的 JSON 配置文件，并通过优先级规则合并为最终配置。

## 配置文件优先级

客户端支持多配置文件管理，按优先级从低到高依次为：

| 文件名 | 优先级 (`__priority`) | 作用说明 |
| --- | --- | --- |
| `main.json` | 0 | 核心应用配置 |
| `system.json` | 1 | 系统级配置（如 WebSocket 通信） |
| `user.json` | 2 | 用户自定义配置（界面外观等） |

覆盖规则：当多个文件中存在相同路径的配置项时，优先级高的文件中的值会覆盖优先级低的文件中的值。

所有配置文件必须包含以下固定字段：

- `"__priority"`: 用于定义优先级（不可删除，值需与文件名对应）。
- `"version"`: 当前版本号为 `"1.0"`。
- `"DGLABClient"`: 固定为 `"DG-LAB-Client"`，用于识别配置文件归属。

## 配置文件详解

### 1. `main.json` —— 核心应用配置

该文件控制客户端的基础行为，包括应用信息、调试选项、日志级别、Python 脚本路径等。

#### 字段说明

| 路径 | 类型 | 说明 |
| --- | --- | --- |
| `app.name` | string | 应用名称（显示在窗口标题等位置） |
| `app.version` | string | 应用版本号 |
| `app.debug` | bool | 是否开启调试模式（Windows 下会创建调试控制台） |
| `app.log.console_level` | int | 控制台日志输出等级：0-DEBUG / 1-INFO / 2-WARN / 3-ERROR / 4-NONE |
| `app.log.only_type_info` | bool | 是否仅输出单个类型日志（用于精简输出） |
| `app.log.ui_log_level` | int | UI 界面日志输出等级（同 console_level 枚举） |
| `app.log.file_enabled` | bool | 是否启用文件日志（默认 `false`）；启用后写入可执行文件目录下的 `log/dglab-YYYYMMDD.log`，按天切分 |
| `app.log.file_level` | int | 文件日志写入等级（同 console_level 枚举），低于该等级的日志不落盘 |
| `app.log.file_max_mb` | int | 单个日志文件大小上限（MB，默认 5）；写满后滚动为 `*.1`，只保留一份备份 |
| `app.module.path` | string | 插件扫描目录（默认 `./module`，相对路径按可执行文件所在目录解析） |
| `app.module.scan_load` | bool | 是否“扫描即加载”：扫描到插件后立即加载（默认 `false`，仅扫描并在界面显示“暂未加载”；开启用于调试或特定场景） |
| `python.path` | string | Python 解释器路径或可执行文件名 |

### 2. `system.json` —— 系统级配置（WebSocket 通信）

该文件用于配置与 DG-LAB 服务交互的 WebSocket 连接参数、消息收发规则等，默认内容如下：

```json
{
    "app": {
        "websocket": {
            "ip": "127.0.0.1",
            "port": 9999
        }
    }
}
```

客户端会根据这里的设置初始化通信模块。

### 3. `user.json` —— 用户自定义配置（界面外观等）

该文件用于存储用户的个性化设置，如界面主题、字号、日志导出设置、默认输出设备等：

```json
{
    "app": {
        "ui": {
            "theme": "classic",
            "font_size": 16
        },
        "output": {
            "default_device": ""
        }
    }
}
```

这些设置会覆盖 `main.json` 和 `system.json` 中的同名项（若存在）。

运行时由插件/模块写入的配置项（经宿主配置接口持久化到本文件）：

| 路径 | 类型 | 说明 |
| --- | --- | --- |
| `app.network.ip_blacklist` | string[] | 中转地址选择的黑名单关键词（网卡名或描述包含则忽略）；缺省时用 `IpSelector` 内置默认值，首次修改后整份名单落盘 |
| `app.network.ip_whitelist` | string[] | 白名单关键词（命中的网卡优先用于自动选择）；缺省时用内置默认值 |
| `app.network.selected_ip` | string | 手动选中的中转地址（空字符串表示按名单自动选择）；非空时优先于自动选择 |
| `app.websocket.ip` / `app.websocket.port` | string / int | 中转服务地址与端口，点「连接」或「选择网卡 IP」确定时写入 |
| `app.output.<A\|B>.target` | string | 通道输出目标，目前只有 `coyote`（郊狼 V3 蓝牙直连）；缺省值与非法值都按 `coyote` 处理，由 `DeviceController::outputTarget()` / `setOutputTarget()` 读写，切目标时停旧目标波形并按当前强度重发 |
| `app.output.default_device` | string | 默认输出设备的蓝牙 MAC（如 `D3:3B:5A:FA:43:0C`，空字符串表示自动评选）。只在该设备在线时生效；离线或用户主动断开时回退到自动评选（郊狼 > 负鼠 > 其它，同类型先连接者优先）。由 `DeviceController::setDefaultDevice()` 写入，界面入口有两处：首页看板的下拉与设备管理页「默认设备」卡（都只列在线输出设备） |
| `app.devices.<MAC>.limit_a` / `safety_a` / `limit_b` / `safety_b` | int | 该设备的通道上下限，优先于通道级默认值。未设置（或该键被删除）时回退到 `app.channels.<A\|B>.{limit, safety}`，再回退到内置默认（`limit=200` / `safety=150`）；读取集中在 `DeviceController::limits_for(mac, channel)`，界面显示与下发都用它算出的 `min(limit, safety)`；界面入口是设备管理页「通道上限与安全上限」卡，写入走 `DeviceController::setDeviceLimit(mac, channel, kind, value)`，每次只写被编辑的那一个键 |
| `app.ui.tray_hint_shown` | bool | Windows 11 托盘折叠区提示是否已经弹过：首次运行（托盘可用）弹一次 Toast 并写 `true`，之后不再提示。图标位置归系统所有，应用无法程序化移到常显区域 |
| `app.devices.<MAC>.experimental_notice_shown` | bool | 实验性设备首次连接成功的「功能未经完整验证」提示是否已经弹过；首次连接时写入 `true`，之后不再提示 |
| `app.devices.<MAC>.model` | string | 该 MAC 记忆的型号标识（`coyote` / `pawprint` / `civet` / `opossum`）：识别时优先于蓝牙名（操作系统可能只给占位名），命中即按该型号的状态处理（郊狼即「已支持」）。首次成功识别出型号时写入（名称命中或连接后首帧反推），之后扫描与连接都直接使用 |
| `app.devices.paw.d0_values` | bool | 爪印 D0 模式（10Hz 全量物理数据）是否写入该实例数值模块（`paw_<MAC 后四位>`，如 `paw_430C`）的 `accel` / `angle_x` / `angle_y` / `angle_z` / `voltage` / `button`；缺省 `false`（10Hz 数据不落值，避免 10Hz 触发规则求值）。键里的 `paw` 是驱动标识，不是设备 MAC，也不随实例改名 |
| `app.devices.civet.pressure_epsilon` | int | 灵猫气压的落值阈值，单位与数值一致（0.01 kPa）；缺省 `10`（即 0.1 kPa）。气压每 100ms 上报一次，只有与上一次落值的差达到阈值才写该实例模块（`civet_<MAC 后四位>`）的 `pressure` / `raw`，其余上报丢弃并计数（其余抖动因此不触发规则求值）；设为 `0` 或负值等于「与原值不同即落值」，即回到 10Hz。键里的 `civet` 是驱动标识，不是设备 MAC；多台灵猫共用这一个阈值 |
| `app.plugins.<模块>.module_migrated_to` | string | 旧模块名（`paw` / `civet`）的事件映射已迁移到哪个实例模块名（如 `paw_430C`）。由 `DeviceValueBridge::migrate_legacy_mapping()` 在第一台实例连上时写一次并保存，旧键与旧值保留；该标记存在时后续设备不再搬同一份映射（见 [docs/devices.md](../docs/devices.md) 的「输入设备实例名与旧配置迁移」） |
| `app.channels.<A\|B>.{limit, safety}` | int | 通道级上下限（所有插件共用），是所有设备的回退层；界面入口在设备管理页的「通道级回退值」卡（设备未单独设置时使用），写入与旧键迁移见 [docs/event-mapping.md](../docs/event-mapping.md) 的「通道上限」 |
| `app.gsi.cs_dir` | string | CS2 GSI 插件记录的 CS 游戏目录（`CS2GsiPlugin` 通过 `PathFinder.py` 查找） |
| `app.gsi.config_path` | string | CS2 GSI 插件生成的 `gamestate_integration_dglab.cfg` 完整路径 |
| `app.gsi.port` | int | CS2 GSI 插件当前监听端口（端口被占用时自动重选并更新） |

> 插件配置读写通过 `IPluginHost::get_config_value`/`set_config_value` 完成，插件无需直接解析配置文件。

### 默认设备与每设备上下限

设备唯一标识取蓝牙 MAC（设备侧固定，重连不变），配置键一律用它拼出。

默认设备的评选只在**有输出能力**的设备之间进行（爪印、灵猫是纯输入设备，不参与），顺序为：

1. 若 `app.output.default_device` 非空且该 MAC 当前在线，就用它。
2. 否则自动评选：类型优先级 郊狼 > 负鼠 > 其它；同类型有多台时先连接的优先。
3. 没有在线输出设备时没有默认设备，通道上下限只按通道级配置（`app.channels.<通道>`）计算。

用户主动断开或设备掉线只会让该设备离开「在线」集合，因此默认设备自动回落到规则 2，配置键本身保留，重新连接后继续生效。

上下限的读取只有一条链，集中在 `DeviceController::limits_for(mac, channel)`：

```text
app.devices.<MAC>.limit_a / safety_a（A 通道）
app.devices.<MAC>.limit_b / safety_b（B 通道）
        ↓ 该键缺失时
app.channels.<A|B>.limit / safety
        ↓ 该键缺失时
内置默认 limit=200 / safety=150
```

界面展示与下发都取这条链的 `min(limit, safety)`（`DeviceController::channel_limit()`），因此切换默认设备后上下限立即跟随。

设备管理页的编辑方式与这条链一致：先选设备（只列在线输出设备），四条输入分别对应 `limit_a` / `safety_a` / `limit_b` / `safety_b`，改动即写盘（只写该键）并触发一次刷新；回读走 `DeviceController::limitsFor(mac, channel)`，两个键都还没落盘时该通道显示通道回退值并标「默认」。

## 热重载

三个配置文件都处于监听状态（文件本身与其所在目录），外部保存后约 300~500 ms 自动重读并整体替换内存配置，`app.devices.<MAC>.*`、`app.channels.<A|B>.*`、`app.output.*` 等键都无需重启即可生效；连续保存会被去抖合并成一次重读。

- **失败保留旧值**：JSON 损坏、缺少 `version` / `DGLABClient`、或 `app.websocket.port` 越界时，客户端保留上一份有效配置并记 WARN，不会把内存配置清空，也不会改写磁盘上的那份坏文件；改回合法内容后自动恢复。
- **删除后重建**：编辑器「先删后建」的保存方式不会丢监听，删除期间保留上一份数据，文件重建后继续生效。
- **自身写盘不触发重读**：客户端保存后会忽略紧随其后的同内容事件，不会出现「保存 → 重读 → 再通知」的循环。
- **保存不会覆盖外部新增的键**：写盘前与磁盘做一次三方合并，外部新增的键并入内存后一起写出；同一个键若应用与外部都改过，以应用的值为准。

## 注意事项

- 配置文件必须为合法的 JSON 格式，否则客户端无法解析（此时按上一节的「失败保留旧值」处理）。
- 如果某个文件缺失，客户端可能无法正常工作，请务必保留三个文件以确保配置完整性。
- 请勿删除或修改 `__priority`、`version`、`DGLABClient` 这三个保留字段。
