# 波形数据结构与 V2 / V3 双数据

> 本文档说明波形模型 `Wave` 的字段、HEX 帧规范、官方内置波形的来源与只读约束、`config/waves/waves.json` 的存储结构，以及发送协议 `app.wave.protocol` 如何选择 V2 / V3 帧。代码实现见 [src/README.md](../src/README.md) 的「wave/ —— 波形」一节与 [include/wave/Wave.h](../include/wave/Wave.h)。

## 一、模型字段

一条波形同时持有 V2 与 V3 两套帧；两套帧各自是权威数据，互不换算。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `name` | string | 波形名，波形库的键；官方内置波形不可改名 |
| `source` | string | 来源：`official`（官方内置资源）或 `user`（用户波形库） |
| `raw` | string | 设计层原始串，形如 `Dungeonlab+pulse:...`；可为空 |
| `sections` | object[] | 段落（起止频率 / 强度），用于再编辑；官方内置波形不带段落 |
| `frames_v2` | string[] | V2 帧序列 |
| `frames_v3` | string[] | V3 帧序列（8 字节 HEX，帧布局见 [include/README.md](../include/README.md) 的「V3 波形帧」） |

- 每帧代表 100ms；`duration_ms() = 帧数 × 100`，帧数优先取 `frames_v3`，只有 V3 缺失时才取 `frames_v2`。
- 段落只生成 V3 帧（`Wave::generate_frames`）：编辑器改段落会清空草稿的 V2 帧，此时发送前按协议校验而不做跨版本换算。

## 二、HEX 帧规范

存储与资源中的帧一律满足：大写、无空白、无分隔符、无 `0x` 前缀。

| 版本 | 长度约束 | 校验入口 |
| --- | --- | --- |
| V3 | 固定 16 个十六进制字符（8 字节） | `Wave::normalize_hex_frame(text, 8)` |
| V2 | 偶数长度合法 HEX，帧长不固定 | `Wave::normalize_hex_frame(text, 0)` |

`normalize_hex_frame` 先去掉空白与 `, ; : - _` 分隔符、去掉开头的 `0x`，再校验字符集与长度，返回大写帧；任一环节不合规返回空串。读取侧一律走它，因此手写的 `0x0a 0a0a 0a00 000000` 会被规范成 `0A0A0A0A00000000`，而 `0A0A0A0A0000ZZ00`（非 HEX）、`0A0A0A0A0000000`（15 字符）、`A1010`（奇数长度）被拒绝。

## 三、官方内置波形

官方 16 个波形来自官方开源示例，原样落在只读资源 `assets/waves/official_waves.json`（登记在 `DGLABClient.qrc` 的 `/waves` 前缀下，路径 `qrc:/waves/assets/waves/official_waves.json`），启动时由 `WaveLibrary` 读取，不在 C++ 里手抄帧数组。

样例字段到内部字段的映射：

| 样例字段 | 内部字段 | 说明 |
| --- | --- | --- |
| `name` | `Wave::name` | 波形名 |
| `raw` | `Wave::raw` | 设计层原始串 |
| `expectedV2` | `Wave::frames_v2` | V2 帧序列 |
| `expectedV3` | `Wave::frames_v3` | V3 帧序列 |

16 个波形与帧数（V2 / V3 帧数相同，合计 693 帧）：

| 波形 | V2 帧数 | V3 帧数 |
| --- | --- | --- |
| 呼吸 | 12 | 12 |
| 潮汐 | 23 | 23 |
| 连击 | 24 | 24 |
| 快速按捏 | 46 | 46 |
| 按捏渐强 | 23 | 23 |
| 心跳节奏 | 35 | 35 |
| 压缩 | 21 | 21 |
| 节奏步伐 | 27 | 27 |
| 颗粒摩擦 | 28 | 28 |
| 渐变弹跳 | 50 | 50 |
| 波浪涟漪 | 57 | 57 |
| 雨水冲刷 | 78 | 78 |
| 变速敲击 | 84 | 84 |
| 信号灯 | 40 | 40 |
| 挑逗1 | 63 | 63 |
| 挑逗2 | 82 | 82 |

加载规则：

- 逐条校验：缺名称、`expectedV2` / `expectedV3` 缺失、任一帧不合规的条目记 `LOG_ERROR` 并跳过，其余条目照常加载（单条坏数据不影响整体）。
- 官方条目 `source = official`，`WaveLibrary::save()` 与 `remove()` 都拒绝它们（不可覆盖、不可删除、不可改名）；从官方波形载入编辑器后另存为新名称即可得到用户副本。
- 官方资源每次启动都读取，用户库文件里的同名条目被官方数据覆盖。
- 旧版内置的占位波形「渐强脉冲」「低频涌动」在加载时移除并记日志；「雨水冲刷」改为官方数据（78 帧），不再由段落推断。

## 四、存储结构

波形库文件 `<wave.path>/waves.json`（默认 `config/waves/waves.json`）：

```json
{
    "version": "1.1",
    "current": { "A": "呼吸", "B": "" },
    "waves": {
        "呼吸": {
            "name": "呼吸",
            "source": "official",
            "raw": "Dungeonlab+pulse:...",
            "sections": [],
            "frames_v2": ["210100", "..."],
            "frames_v3": ["0A0A0A0A00000000", "..."]
        }
    }
}
```

- 官方内置波形同样写入库文件，因此「加载 → 落盘 → 重新加载」后 `raw` / `frames_v2` / `frames_v3` 完全一致；重新加载时仍以资源为准。
- 旧格式（只有 `frames` 的 V3 数组、没有 `frames_v2` / `source`）按 `frames_v3` 读入，`source` 按 `user` 处理，数据不丢。
- 条目里没有任何合规帧时整条跳过并记 `LOG_WARN`；部分帧不合规只丢那些帧。

## 五、发送协议与缺帧提示

发送协议写在 `user.json` 的 `app.wave.protocol`，取值 `v2` / `v3`，缺省 `v3`；非法取值忽略并按 `v3` 生效。

- 发送路径按协议取对应帧：`WaveLibrary::frames_of(name, protocol)`，`v2` 取 `frames_v2`、`v3` 取 `frames_v3`。
- 目标版本缺帧时**不回落另一版本**、也不发送空数据：`WaveBridge` 写 `lastError`（`波形《X》缺少 V2 帧，未发送` / `当前发送协议为 V2，草稿缺少 V2 帧，未发送`）并记 `LOG_WARN`，界面经既有的「`lastError` 由空转非空 → Toast（danger）」路由提示用户。
- `WaveBridge::waves` 的列表项带 `sendable`（当前协议是否具备帧）、`official`、`frameCountV2` / `frameCountV3`，供界面禁用发送入口。
- 波形预览（`pointsA` / `pointsB` / `amplitudeA` / `amplitudeB`）与事件包络始终取 V3 帧：预览按 V3 的 4×25ms 子单元解析强度与频率。

## 六、测试覆盖

`tests/wave_library_test.cpp`（ctest 目标 `wave_library_test`）逐项验证：

- 16 个官方内置波形全部加载，名称集合与样例一致，且 `raw` 非空、无段落；
- 「呼吸」「潮汐」「雨水冲刷」的 `frames_v2` / `frames_v3` 与样例逐帧相等；
- 存储 round-trip：落盘后重新加载，`raw` / `frames_v2` / `frames_v3` 一致；
- 旧格式只有 `frames` 的条目读入 `frames_v3`；
- 非法帧（长度错误、非 HEX）被拒绝，无合规帧的条目整条跳过；
- 官方波形不可覆盖 / 删除，旧占位波形已移除；
- 协议切换与缺帧不回落。
