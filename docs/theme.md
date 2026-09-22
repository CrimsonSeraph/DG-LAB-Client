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
| `hover` / `pressed` | color | 强调色控件悬停 / 按下态 |
| `selectionBg` | color | 选中项背景（导航选中、节点选中、下拉高亮） |
| `success` / `warning` / `danger` | color | 语义色，逐主题校正到在 `surface` 上可读 |
| `presets` | list | 预设主题数组：`{ mode, name, primary, secondary, dark }` |

---

## 三、内置预设（16 套）

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

> 「午夜蓝」「森野绿」为主题扩展新增；自定义主/副色不进入 `presets`（`custom` 为独立模式）。

---

## 四、派生令牌与对比度

`recompute()` 的派生步骤：

1. 由 `primary` / `secondary` 的亮度分出「底色」与「强调色」；`dark` 决定底色偏白还是偏黑。
2. 先校正背景：浅色主题保证纯黑文字可达 7.05:1，深色主题保证纯白文字可达 7.05:1（`ensure_background`），必要时把底色压暗/提亮。
3. 再校正文字：`textPrimary` 在 `surface` 上达 7:1；`textSecondary` / `textMuted` 在 `surface` 与 `surfaceAlt` 上达 4.5:1（`ensure_contrast`）。
4. 校正 `accent`、`accentText`、`border`、`divider` 与 `success` / `warning` / `danger`。

下表为 16 套预设计算后的关键令牌（近似整数值，实际以运行时 `QColor` 为准）：

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
| `accentText` / `accent`                    | 4.5:1  | 强调色按钮上的文字               |

约束由 `ThemeManager.cpp` 中的 `relative_luminance` / `contrast_ratio` / `ensure_contrast` / `ensure_background` 保证：对不达标的前景/背景按 1% 步进向黑或白逼近，直到达标；由于黑白极值通常足以满足上述下限，结果确定且与主题数量无关。

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
| `AppTextField` | `TextField` | 文字/占位符/选区/背景主题化，聚焦描边转强调色 |
| `AppTextArea`  | `TextArea`  | 同上，用于表达式等长文本                      |
| `AppComboBox`  | `ComboBox`  | 内容区、指示器、弹层、委托全部主题化          |
| `AppCheckBox`  | `CheckBox`  | 指示器与文字主题化，选中态用强调色            |

新增输入控件时复用这些封装，不要直接使用 Qt 原生控件；尺寸常量放在 `ComponentStyle`。

---

_最后更新：2026-09-18_
