# 样式文件目录（qcss）

本目录存放 Qt 样式表（QSS）文件，用于定义应用程序的视觉外观。

---

## 文件说明

本目录包含 **14 个**样式文件，分别对应浅色模式、深色模式及 **12 个扩展主题**。

### 基础主题

| 文件名 | Theme | Mode 属性 | 颜色方案（主色 / 副色） |
| - | - | - | - |
| `light.qcss` | Light (浅色模式) | `light` | Ice Blue #E8F0FE, Misty Blue #D4E6F1 |
| `night.qcss` | Night (深色模式) | `night` | Deep Slate #2C3E50, Dark Navy #1A252F |

### 扩展主题（12 种）

| 文件名 | Theme | Mode 属性 | 颜色方案（主色 / 副色） |
| - | - | - | - |
| `style_charcoal_pink.qcss` | Charcoal Pink (炭黑甜粉) | `charcoal_pink` | Charcoal #1A1A1D, Sweet Pink #E6397C |
| `style_deepsea_cream.qcss` | Deepsea Cream (深海奶白) | `deepsea_cream` | Deepsea Blue #122E8A, Soft Milk White #F5EFEA |
| `style_vine_purple_tea_green.qcss` | Vine Purple Tea Green (藤紫钛绿) | `vine_purple_tea_green` | Vine Purple #91C53A, Titanium Green #5E55A2 |
| `style_offwhite_camellia.qcss` | Offwhite Camellia (无白茶花) | `offwhite_camellia` | Offwhite #F1DDDF, Camellia Red #E72D48 |
| `style_dark_blue_clear_blue.qcss` | Dark Blue Clear Blue (捣蓝清水) | `dark_blue_clear_blue` | Dark Blue #113056, Clear Blue #91D5D3 |
| `style_klein_yellow.qcss` | Klein Yellow (克莱因黄) | `klein_yellow` | Klein Blue #002EA6, Pine Flower Yellow #FFE76F |
| `style_mars_green_rose.qcss` | Mars Green Rose (马尔斯玫瑰) | `mars_green_rose` | Mars Green #01847F, Rose Pink #F9D2E4 |
| `style_hermes_orange_navy.qcss` | Hermes Orange Navy (爱马仕深蓝) | `hermes_orange_navy` | Hermes Orange #FF770F, Navy Blue #000026 |
| `style_tiffany_blue_cheese.qcss` | Tiffany Blue Cheese (蒂芙尼奶酪) | `tiffany_blue_cheese` | Tiffany Blue #81D8CF, Cheese Yellow #F8F5D6 |
| `style_china_red_yellow.qcss` | China Red Yellow (中国红黄) | `china_red_yellow` | China Red #FF0000, Light Yellow #FAEAD3 |
| `style_vandyke_brown_khaki.qcss` | Vandyke Brown Khaki (凡戴克棕卡其) | `vandyke_brown_khaki` | Vandyke Brown #492D22, Light Khaki #D8C7B5 |
| `style_prussian_blue_fog.qcss` | Prussian Blue Fog (普鲁士雾灰) | `prussian_blue_fog` | Prussian Blue #003153, Fog Gray #E5DDD7 |

> **注意**：所有样式文件统一使用 `rgba` 颜色值，并遵循 60% 主色 + 30% 副色 + 10% 点缀色的比例原则。扩展主题的 `mode` 属性值（如 `charcoal_pink`）与文件名中的标识一致。

---

## 控件属性选择器

样式表中使用以下自定义属性来匹配不同控件，不依赖具体类名或 ID，便于主题扩展。

### `type` 属性（控件类型）

| 属性值 | 控件类型 | 说明 |
| - | - | - |
| `glass_panel` | 玻璃拟态容器 | 半透明背景，圆角，轻微边框 |
| `glass_panel_inner` | 内层玻璃面板 | 更高透明度，无边框，用于嵌套 |
| `main_page` / `config_page` 等 | 页面背景 | 页面级半透背景，大圆角 |
| `title` | 标题文字 | 大号加粗，颜色略深 |
| `subtitle` | 副标题文字 | 中号常规，颜色稍浅 |
| `strength_card` | 强度卡片 | 高透明度白色背景，小圆角 |
| `wave_card` | 波形卡片 | 半透背景，边框 |
| `info_card` | 信息卡片 | 与玻璃面板相同风格 |
| `debug_log` | 日志文本框 | 等宽字体，半透背景 |
| `channel_panel` | 通道控制面板 | 同玻璃面板 |
| `port_bar` | 端口栏 | 同玻璃面板 |
| `theme_card` | 主题卡片 | 同玻璃面板 |

### `button_type` 属性（按钮风格）

| 属性值 | 按钮类型 | 说明 |
| - | - | - |
| `special` | 特殊按钮 | 高亮背景（如黄色/橙色），用于重要操作 |
| `emphasis` | 强调按钮 | 红色系，用于危险或确认操作 |
| *(无该属性)* | 普通按钮 | 默认灰白背景，悬停加深 |

### `font_size` 属性（字体大小）

| 属性值 | 字号 |
| - | - |
| `S` | 9px |
| `M` | 12px（默认） |
| `L` | 18px |

示例样式片段（浅色模式下的特殊按钮）：

```css
QPushButton[button_type="special"][theme="light"] {
    background: rgba(255,235,0,0.6);
    border-color: rgba(230,180,34,1.0);
    font-weight: bold;
}
QPushButton[button_type="special"][theme="light"]:hover {
    background: rgba(255,255,0,0.6);
}
```

深色模式下的相同按钮（`theme="night"`）：
```css
QPushButton[button_type="special"][theme="night"] {
    background: rgba(255,200,50,0.25);
    border-color: rgba(255,180,30,0.8);
}
```

---

## 使用说明

应用程序通过 `AppConfig` 读取当前选中的主题（`Theme` 枚举值）。在 `DGLABClient` 初始化时，根据配置加载对应的样式文件，并调用 `apply_widget_properties()` 为所有需要样式的控件设置 `type`、`button_type` 和 `theme` 属性。

**切换主题**：
- 点击界面上的“切换主题”按钮，弹出 `ThemeSelectorDialog` 选择主题。
- 内部修改配置中的 `theme` 值，调用 `change_theme()` 重新加载样式表并刷新所有控件的 `theme` 属性。

**动态设置示例**：
```cpp
QString themeStr = theme_to_mode_string(current_theme);
qApp->setProperty("theme", themeStr);
// 对每个需要应用样式的顶层控件也设置 theme
mainWindow->setProperty("theme", themeStr);
// 重新加载样式文件
load_stylesheet(themeStr);
```

**注意事项**：
- 所有自定义控件（如 `SampledWaveformWidget`）通过 `type` 或 `id` 选择器设置背景和圆角。
- 样式文件中的 `theme` 属性由代码动态设置，与 `type` / `button_type` 属性共同作用。
- 如需添加新的控件类型，请在 `apply_widget_properties()` 中为对应控件设置唯一属性值，并在所有样式文件中定义相应规则。
- 扩展主题的样式文件命名规则为 `style_<mode>.qcss`，其中 `<mode>` 与 `theme` 属性值保持一致（如 `charcoal_pink`）。

---

## 维护与扩展

- 新增主题时，请参照现有主题的格式创建一个新的 QSS 文件，并在此 README 中更新主题列表。
- 所有主题必须支持相同的 `type` / `button_type` 属性集合，以保证界面布局不因主题切换而错位。
- 建议使用 `rgba` 颜色值以便统一透明度控制，渐变背景可使用 `qlineargradient`，但当前主题以纯色半透明为主。
- 主题颜色映射定义在 `ThemeSelectorDialog` 的 `primary_colors_` 和 `secondary_colors_` 中，新增主题时需同步更新这两个映射。
