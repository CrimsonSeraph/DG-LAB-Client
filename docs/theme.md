# 主题系统（Theme）

> 本文档描述 QML 界面的颜色令牌、内置预设配色、对比度约束与扩展方式。代码入口：`include/ui/ThemeManager.h` 与 `src/ui/ThemeManager.cpp`，在 `main.cpp` 中以 `qmlRegisterSingletonInstance` 注册为 QML 单例 `Theme`。

---

## 一、设计原则

- **颜色只在 C++ 定义**：QML 只引用 `Theme.xxx` 语义令牌，页面与组件里不写 `"white"` / `"black"` / `#RRGGBB` 等颜色字面量。
- **运行时重算**：`ThemeManager::recompute()` 由「主色 + 副色 + 明暗」派生全部令牌；切换主题只更新 `Theme`，QML 的属性绑定自动刷新，无需重建界面。
- **可读性优先**：文字、边框与语义色逐主题做 WCAG 对比度校正，浅色主题下也不会出现浅底浅字或深底深字。
- **令牌分工**：颜色在 `Theme`（C++）、字号在 `Typography`、通用尺寸在 `Metrics`、组件专属度量在 `ComponentStyle`。

---

## 二、令牌清单

| 令牌 | 类型 | 说明 |
| --- | --- | --- |
| `themeName` | string | 当前主题英文模式名（如 `light`、`charcoal_pink`、`custom`） |
| `displayName` | string | 当前主题中文名（如「浅色模式」） |
| `custom` | bool | 是否为自定义主/副色主题 |
| `dark` | bool | 明暗判定（决定文字/背景的派生方向） |
| `primary` / `secondary` | color | 用户选择的原始主色与副色 |
| `windowBg` | color | 窗口/页面底色（最外层背景） |
| `surface` | color | 卡片、对话框、画布底色 |
| `surfaceAlt` | color | 次级表面：区块、输入框、节点卡片 |
| `surfaceSubtle` | color | 最轻的表面：微弱分区 |
| `border` | color | 卡片/控件的描边色 |
| `divider` | color | 分隔线与网格线（画布网格以 0.4 不透明度绘制） |
| `textPrimary` | color | 主文字（对 `surface` 达 WCAG AAA） |
| `textSecondary` | color | 次级文字（对 `surface` / `surfaceAlt` 达 AA） |
| `textMuted` | color | 弱化文字（对 `surface` / `surfaceAlt` 达 AA） |
| `accent` | color | 强调色：选中态、焦点描边、连线、数值高亮 |
| `accentText` | color | 画在 `accent` 之上的对比文字 |
| `hover` / `pressed` | color | 强调色控件悬停 / 按下态（按下朝背离 `accentText` 的方向多走 0.42，比悬停更明显一档且文字对比度随按压上升） |
| `selectionBg` | color | 选中项背景：`surface` 与 `accent` 调出的着色带（对 `surface` ≥1.20:1，可读成一条带） |
| `selectionText` | color | 选中项文字（对 `selectionBg` ≥4.5:1）；与 `selectionBg` 成对使用 |
| `success` / `warning` / `danger` | color | 语义色，逐主题校正到在 `surface` 上可读 |
| `focusRing` | color | 焦点环（对 `surface` / `surfaceAlt` 均 ≥3:1），由 `accent` 派生 |
| `textDisabled` | color | 禁用态文字，由 `textMuted` 派生（对 `surfaceDisabled` ≥3:1） |
| `surfaceDisabled` | color | 禁用态底色，由 `surface` 与 `surfaceSubtle` 各半混合 |
| `borderDisabled` | color | 禁用态描边，由 `border` 派生（对 `surfaceDisabled` ≥1.35:1） |
| `surfacePressed` | color | 中性控件按下底色：`surfaceAlt` 压暗 8%，其上 `textPrimary` ≥4.5:1 |
| `transparentFill` | color | 完全透明（供 `Shape` 填充、无描边按钮等使用，替代 QML 里的 `"transparent"`） |
| `dangerText` | color | 危险态填充文字，对 `danger` / `dangerHover` / `dangerPressed` 均 ≥4.5:1 |
| `dangerHover` | color | 危险态悬停底色，由 `danger` 朝远离 `surface` 方向混 10% |
| `dangerPressed` | color | 危险态按下底色：`danger` 向 `surface` 混 12%，再按填充文字侧校正到 AA |
| `dangerOnAlt` / `warningOnAlt` / `successOnAlt` | color | 语义色在 `surfaceAlt` 上的校正版本（≥4.5:1） |
| `presets` | list | 预设主题数组：`{ mode, name, primary, secondary, dark }` |

---

## 三、内置预设（17 套）

每个预设只需定义 `primary` / `secondary` 与明暗，其余令牌由 `recompute()` 派生。

| 中文名       | mode                    | primary   | secondary | 明暗 |
| ------------ | ----------------------- | --------- | --------- | ---- |
| 浅色模式     | `light`                 | `#E8F0FE` | `#D4E6F1` | 浅色 |
| 深色模式     | `night`                 | `#2C3E50` | `#1A252F` | 深色 |
| 炭黑甜粉     | `charcoal_pink`         | `#1A1A1D` | `#E6397C` | 深色 |
| 深海奶白     | `deepsea_cream`         | `#122E8A` | `#F5EFEA` | 浅色 |
| 藤紫钛绿     | `vine_purple_tea_green` | `#91C53A` | `#5E55A2` | 深色 |
| 无白茶花     | `offwhite_camellia`     | `#F1DDDF` | `#E72D48` | 浅色 |
| 捣蓝清水     | `dark_blue_clear_blue`  | `#113056` | `#91D5D3` | 深色 |
| 克莱因黄     | `klein_yellow`          | `#002EA6` | `#FFE76F` | 深色 |
| 马尔斯玫瑰   | `mars_green_rose`       | `#01847F` | `#F9D2E4` | 深色 |
| 爱马仕深蓝   | `hermes_orange_navy`    | `#FF770F` | `#000026` | 深色 |
| 蒂芙尼奶酪   | `tiffany_blue_cheese`   | `#81D8CF` | `#F8F5D6` | 浅色 |
| 中国红黄     | `china_red_yellow`      | `#FF0000` | `#FAEAD3` | 浅色 |
| 凡戴克棕卡其 | `vandyke_brown_khaki`   | `#492D22` | `#D8C7B5` | 浅色 |
| 普鲁士雾灰   | `prussian_blue_fog`     | `#003153` | `#E5DDD7` | 浅色 |
| 午夜蓝       | `midnight_blue`         | `#142240` | `#5B8DEF` | 深色 |
| 森野绿       | `forest_green`          | `#16332A` | `#7BC98A` | 深色 |
| 经典         | `classic`               | `#111111` | `#F1E3A6` | 深色 |

> 「午夜蓝」「森野绿」为主题扩展新增；自定义主/副色不进入 `presets`（`custom` 为独立模式）。

---

## 四、派生令牌与对比度

`recompute()` 的派生步骤：

1. 由 `primary` / `secondary` 的亮度分出「底色」与「强调色」；`dark` 决定底色偏白还是偏黑。
2. 先校正背景：浅色主题保证纯黑文字可达 7.05:1，深色主题保证纯白文字可达 7.05:1（`ensure_background`），必要时把底色压暗/提亮。
3. 再校正文字：`textPrimary` 在 `surface` 上达 7:1；`textSecondary` / `textMuted` 在 `surface` 与 `surfaceAlt` 上达 4.5:1（`ensure_contrast`）。
4. 校正 `accent`、`accentText`、`border`、`divider` 与 `success` / `warning` / `danger`。
5. 派生强调态交互色：`hover` = `accent` 朝背离底色方向混 14%（文字对比度只升不降）；`pressed` = `accent` 朝**背离 `accentText`** 的方向混 42%（`accentText` 浅则压暗、深则提亮），因此按下比悬停更明显一档，且 `accentText` 在按下态对比度单调上升，无需搜索。
6. 派生选中态：`selectionBg` = `surface` 与 `accent` 的着色带（深色 0.30 / 浅色 0.16），先加大系数保证对 `surface` ≥1.20:1；若其上 `textPrimary` 不足 4.5:1，再朝背离文字的方向推移到两条同时成立。`selectionText` = `accent` 对 `selectionBg` 校正到 4.5:1（着色带会压低 `accent` 的对比度，故选中等文字单独一档）。
7. 派生焦点与禁用令牌：`focusRing` = 校正后的 `accent`（对两个表面 ≥3:1）；`surfaceDisabled`、`textDisabled`、`borderDisabled` 依次派生并在底色上校验。
8. 派生危险态填充：`dangerText` 取黑/白中在 `danger` 上达标者（任意底色至少一端可达标）；`dangerHover` 混 10%；`dangerPressed` 先向 `surface` 混 12%，若不达 AA 再朝远离文字的方向推移。

下表为 17 套预设计算后的关键令牌（近似整数值，实际以运行时 `QColor` 为准）：

| mode | surface | surfaceAlt | textPrimary | textSecondary | textMuted | accent | border | textPrimary/surface |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `light` | `#F6F9FF` | `#F2F7FE` | `#4F565A` | `#697176` | `#6B7175` | `#697278` | `#D0D5D8` | 7.10 |
| `night` | `#1A252F` | `#2C3640` | `#E6E8EA` | `#A6AEB5` | `#969FA8` | `#959EA7` | `#4C555D` | 12.65 |
| `charcoal_pink` | `#1A1A1D` | `#2C2C2F` | `#FCE7EF` | `#F5ACC8` | `#F084AE` | `#EC6599` | `#4C4C4F` | 14.76 |
| `deepsea_cream` | `#FBF9F7` | `#FAF6F3` | `#08153E` | `#1E3890` | `#596DAD` | `#122E8A` | `#B3BCDA` | 16.87 |
| `vine_purple_tea_green` | `#574F97` | `#655D9F` | `#FEFEFD` | `#D7EAB7` | `#D7EAB7` | `#D7EAB7` | `#7C76AE` | 7.00 |
| `offwhite_camellia` | `#FAF2F3` | `#F7ECED` | `#681420` | `#C42F45` | `#A84C59` | `#CE2840` | `#F7BCC4` | 11.14 |
| `dark_blue_clear_blue` | `#113056` | `#244164` | `#F2FAFA` | `#D1EDED` | `#BBE5E4` | `#91D5D3` | `#455E7B` | 12.53 |
| `klein_yellow` | `#002EA6` | `#143FAD` | `#FFFCEE` | `#FFF5C3` | `#FFF0A6` | `#FFE76F` | `#385CBA` | 10.52 |
| `mars_green_rose` | `#01635F` | `#156F6C` | `#FFFDFE` | `#FCECF4` | `#FBE3EE` | `#FAD7E7` | `#398582` | 7.00 |
| `hermes_orange_navy` | `#000026` | `#141437` | `#FFEFE2` | `#FFC69A` | `#FFAB6A` | `#FF770F` | `#383856` | 18.15 |
| `tiffany_blue_cheese` | `#FCFBEF` | `#FBFAE8` | `#385D59` | `#4C7A75` | `#587774` | `#4A7B76` | `#C3DDDA` | 7.00 |
| `china_red_yellow` | `#FDF7EE` | `#FCF3E7` | `#730000` | `#DC0B0B` | `#C63B3B` | `#E10000` | `#FFADAD` | 11.43 |
| `vandyke_brown_khaki` | `#F0EAE3` | `#EAE0D6` | `#21140F` | `#52382D` | `#6C5C55` | `#492D22` | `#C5BCB8` | 14.97 |
| `prussian_blue_fog` | `#F5F2F0` | `#F1ECE9` | `#001625` | `#0D3B5C` | `#4B6D84` | `#003153` | `#ADBDC8` | 16.50 |
| `midnight_blue` | `#142240` | `#27344F` | `#EBF1FD` | `#BACFF8` | `#99B8F5` | `#6F9BF1` | `#48536A` | 13.94 |
| `forest_green` | `#16332A` | `#29433B` | `#EFF9F1` | `#C8E8CE` | `#ADDEB6` | `#7BC98A` | `#496059` | 12.61 |
| `classic` | `#111111` | `#242424` | `#FDFCF4` | `#F9F3DA` | `#F6EEC8` | `#F1E3A6` | `#454545` | 18.32 |

---

## 五、对比度约束

| 约束                                       | 下限   | 说明                             |
| ------------------------------------------ | ------ | -------------------------------- |
| `textPrimary` / `surface`                  | 7.0:1  | WCAG AAA，正文主色               |
| `textPrimary` / `surfaceAlt`、`windowBg`   | 4.5:1  | 次级背景上的主文字仍可读         |
| `textSecondary` / `surface`、`surfaceAlt`  | 4.5:1  | WCAG AA                          |
| `textMuted` / `surface`、`surfaceAlt`      | 4.5:1  | WCAG AA                          |
| `border` / `surface`、`surfaceAlt`         | 1.35:1 | 描边可辨（非文字，按可感知下限） |
| `divider` / `surface`、`surfaceAlt`        | 1.15:1 | 分隔线/网格可辨                  |
| `danger`、`warning`、`success` / `surface` | 4.5:1  | 语义色作为文字/描边时可读        |
| `accentText` / `accent`、`hover`、`pressed` | 4.5:1  | 强调色按钮三种状态的填充文字     |
| `selectionBg` / `surface`                 | 1.20:1 | 选中带相对底色可辨               |
| `textPrimary` / `selectionBg`             | 4.5:1  | 下拉委托等画在选中带上的主文字   |
| `selectionText` / `selectionBg`           | 4.5:1  | 导航选中态文字与其背景           |
| `dangerText` / `danger`、`dangerPressed` | 4.5:1  | 危险按钮填充文字与其底色         |
| `dangerOnAlt` / `warningOnAlt` / `successOnAlt` / `surfaceAlt` | 4.5:1 | 语义色画在次级表面上 |
| `focusRing` / `surface`、`surfaceAlt`     | 3.0:1  | 焦点环（WCAG 非文本）            |
| `textDisabled` / `surfaceDisabled`        | 3.0:1  | 禁用态文字（WCAG 非文本下限）    |
| `borderDisabled` / `surfaceDisabled`      | 1.35:1 | 禁用态描边可辨                   |
| `surfacePressed` / `textPrimary`           | 4.5:1  | 中性控件按下态文字               |

约束由 `ThemeManager.cpp` 中的 `relative_luminance` / `contrast_ratio` / `ensure_contrast` / `ensure_background` / `selection_band` 保证：对不达标的前景/背景按 1% 步进向黑或白逼近，直到达标；由于黑白极值通常足以满足上述下限，结果确定且与主题数量无关。`selectionText` 的搜索目标取 4.55:1，留出 16 位通道量化余量。

### 5.1 实测达成值

按 `recompute()` 复现计算后的关键组合（17 套预设全部 ≥ 下限，下表取三套代表）：

| 组合 | 下限 | 浅色模式 | 深色模式 | 炭黑甜粉 |
| --- | --- | --- | --- | --- |
| `accentText` / `accent` | 4.5 | 4.91 | 5.26 | 4.67 |
| `accentText` / `hover` | 4.5 | 6.21 | 6.19 | 5.44 |
| `accentText` / `pressed` | 4.5 | 10.16 | 8.36 | 7.51 |
| `selectionBg` / `surface` | 1.20 | 1.22 | 1.70 | 1.60 |
| `textPrimary` / `selectionBg` | 4.5 | 5.84 | 7.42 | 9.23 |
| `selectionText` / `selectionBg` | 4.5 | 4.60 | 4.58 | 4.59 |
| `hover` / `pressed`（按下可见性） | — | 1.64 | 1.35 | 1.38 |
| `dangerText` / `danger` | 4.5 | 5.44 | 6.15 | 5.51 |
| `dangerText` / `dangerHover` | 4.5 | 6.40 | 7.03 | 6.36 |
| `dangerText` / `dangerPressed` | 4.5 | 4.52 | 5.16 | 4.61 |
| `dangerOnAlt` / `surfaceAlt` | 4.5 | 5.05 | 4.55 | 4.52 |
| `warningOnAlt` / `surfaceAlt` | 4.5 | 4.55 | 4.51 | 4.54 |
| `successOnAlt` / `surfaceAlt` | 4.5 | 4.64 | 4.55 | 4.51 |
| `textDisabled` / `surfaceDisabled` | 3.0 | 4.77 | 5.53 | 6.86 |
| `focusRing` / `surface` | 3.0 | 4.66 | 5.74 | 5.68 |
| `borderDisabled` / `surfaceDisabled` | 1.35 | 1.43 | 1.97 | 1.97 |

17 套预设扫描结论：上述组合**全部达标**；主文字令牌（`textPrimary` / `textSecondary` / `textMuted` / `accent` 对 `surface`）的派生未改动，浅色模式 7.10 / 4.71 / 4.71 / 4.66、深色模式 12.65 / 6.93 / 5.76 / 5.74 与改造前一致。

选中态由**两个令牌成对**承担：`selectionBg`（着色带）+`selectionText`（选中文字）。`selectionBg` 上只画 `textPrimary`（下拉委托）或 `selectionText`（导航选中），不再出现 `accentText`，因此无需让同一个背景同时对两极达标。四套代表主题的取值：

| mode | `selectionBg` | `selectionText` | `textPrimary` | `accent` | `pressed` |
| --- | --- | --- | --- | --- | --- |
| `light` | `#E0E4E9` | `#5E656A` | `#4F565A` | `#697277` | `#3D4245` |
| `night` | `#3F4953` | `#B2B8BF` | `#E6E8EA` | `#959EA7` | `#C2C7CC` |
| `charcoal_pink` | `#593142` | `#F088B1` | `#FCE7EF` | `#EC6599` | `#F4A6C4` |
| `classic` | `#54503E` | `#F1E3A6` | `#FDFCF4` | `#F1E3A6` | `#F7EFCB` |

按下态：`pressed` 朝背离 `accentText` 的方向混 42%，`accentText` 在按下态的对比度随之升到 7.5:1 以上，且按下比悬停明显更深/更亮一档（三套代表主题 `hover`/`pressed` 对比度 1.64 / 1.35 / 1.38）。17 套中最紧的是克莱因黄、藤紫钛绿、马尔斯玫瑰、经典（1.06–1.10）：它们的 `accent` 本就接近白色，`accentText` 为深色，「背离文字」的方向很快撞到白色上限；再往深处走会跌破 4.5:1，故取物理可达的最大步进。

---

## 六、如何新增主题

1. 在 `src/ui/ThemeManager.cpp` 的 `build_presets()` 中追加一行：

    ```cpp
    { QStringLiteral("my_theme"), QStringLiteral("我的主题"), QColor(0x12, 0x34, 0x56), QColor(0xAB, 0xCD, 0xEF), true },
    ```

    字段依次为：`mode`（英文名，持久化与 `applyPreset` 使用）、`name`（中文名）、`primary`、`secondary`、`dark`。

2. 同步更新 `include/ui/ThemeManager.h` 注释中的预设数量，以及本文档第三节、第四节的表格。
3. 无需改 QML：预设自动出现在配置页「选择主题」对话框中（`Theme.presets`），点击后由 `UiConnector` 调用 `Theme.applyPreset(mode)` 即时生效。

> 选择 `dark` 时，`ensure_background` 会把底色压暗到足以承载近白文字；若主色本身偏亮，底色会比主色更深，这是为满足对比度下限的有意行为。

---

## 七、自定义主题入口

- 配置页「自定义主题」按钮打开 `customThemeDialog`（`objectName`），通过 `ColorDialog` 选择主/副色。
- 保存时 `UiConnector` 调用 `Theme.applyCustom(primary, secondary)`，主题名记为 `custom`。
- 选择结果持久化到 `user.json` 的 `app.ui.custom.primary` / `app.ui.custom.secondary`；`app.ui.theme` 记录当前模式名。
- 自定义主题与预设走同一套派生与对比度校正逻辑。

---

## 八、控件约定

原生控件不带主题，输入类控件统一使用 `src/ui/qml/components/` 下的封装（全部从 `Theme` 取色）：

| 控件           | 基于        | 说明                                          |
| -------------- | ----------- | --------------------------------------------- |
| `AppSpinBox`   | `SpinBox`   | 内容区、上下指示器、背景全部主题化            |
| `AppTextField` | `TextField` | 文字/占位符/选区/背景主题化，聚焦描边转焦点环 |
| `AppTextArea`  | `TextArea`  | 同上，用于表达式等长文本                      |
| `AppComboBox`  | `ComboBox`  | 内容区、指示器、弹层、委托全部主题化          |
| `AppCheckBox`  | `CheckBox`  | 指示器与文字主题化，选中态用强调色            |
| `AppButton` / `NavButton` | `Button` | 强调/危险/中性三态，按下用 `pressed` / `dangerPressed` / `surfacePressed`；导航选中态为 `selectionBg` + `selectionText` |

组件状态约定（尺寸与透明度取 `Metrics.focusRingWidth` / `focusRingOffset` / `disabledOpacity`）：

| 状态 | 表现 |
| --- | --- |
| 焦点 | `activeFocus` 或 `visualFocus` 时画 `focusRing` 描边，宽 `Metrics.focusRingWidth` |
| 禁用 | `enabled === false`：底色 `surfaceDisabled`、文字 `textDisabled`、描边 `borderDisabled`，透明度 `Metrics.disabledOpacity` |
| 只读 | 输入框保留正常底色与描边，文字降为 `textSecondary`（与禁用态可区分） |

新增输入控件时复用这些封装，不要直接使用 Qt 原生控件；尺寸常量放在 `ComponentStyle`。

---

_最后更新：2026-09-18_
