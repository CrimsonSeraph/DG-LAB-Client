# 样式文件目录（qcss）

本目录存放 Qt 样式表（QSS）文件，用于定义应用程序的视觉外观。

---

## 文件说明

本目录包含 **14 个**样式文件，分别对应浅色模式、暗夜模式及 **12 个扩展主题**。

### 基础主题

| 文件名 | Theme | Mode 属性 | 颜色方案 |
| - | - | - | - |
| `light.qcss` | Light (浅色模式) | `light` | Ice Blue #E8F0FE, Misty Blue #D4E6F1 |
| `night.qcss` | Night (深色模式) | `night` | Deep Slate #2C3E50, Dark Navy #1A252F |

### 扩展主题（12 种）

| 文件名 | Theme | Mode 属性 | 颜色方案 |
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

> **注意**：样式文件中使用了统一的 `type` 属性选择器（如 `[type="nav_btn"]`）和 `theme` 属性选择器（如 `[theme="charcoal_pink"]`），不同主题仅通过 `theme` 值区分配色。

---

## 控件类型（type 属性）

无论使用哪个主题，以下 `type` 属性值均被支持，用于匹配对应的控件样式：

| 属性值 | 控件类型 | 说明 |
| - | - | - |
| `main_page` | 主窗口背景 | 线性渐变背景（颜色随主题变化） |
| `glassmorphism` | 玻璃拟态容器 | 半透明背景，边框，圆角 |
| `nav_btn` | 导航按钮 | 渐变背景，白色文字，圆角 |
| `action_btn` | 操作按钮 | 次级按钮风格，带边框 |
| `title` | 标题文字 | 大号加粗，居中 |
| `label` | 普通标签 | 常规字体，左对齐 |
| `image` | 图片标签 | 半透背景，圆角，文字居中 |
| `input` | 输入框 | 实色背景，边框，焦点高亮 |
| `debug_log` | 日志文本框 | 深色背景，等宽字体 |
| `scrollarea` | 滚动区域 | 透明背景，无边框 |
| `stacked` | 堆叠窗口 | 透明背景 |
| `channel_panel` | 通道面板 | 半透背景，圆角 |
| `port_panel` | 端口面板 | 半透背景，圆角 |
| `button_bar` | 按钮栏 | 极浅半透背景 |
| `sub_panel` | 子面板 | 完全透明，用于布局 |
| `waveform` | 波形控件（自定义） | 深色背景，圆角 |

示例样式片段（浅色模式导航按钮）：

```css
QPushButton[type="nav_btn"][theme="light"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 rgba(108,142,191,1),
                                stop:1 rgba(74,106,142,1));
    border: none;
    border-radius: 8px;
    font-size: 16px;
    font-weight: bold;
    color: white;
    padding: 8px 12px;
}
QPushButton[type="nav_btn"][theme="light"]:hover {
    background: qlineargradient(...);
}
```

暗夜模式示例（相同的 `type`，不同的 `theme`）：
```css
QPushButton[type="nav_btn"][theme="night"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 rgba(58,90,126,1),
                                stop:1 rgba(42,74,110,1));
    color: #e0e0e0;
}
```

---

## 使用说明

应用程序通过 `AppConfig` 读取当前选中的主题（存储为 `Theme` 枚举值，如 `LIGHT`, `NIGHT`, `CHARCOAL_PINK` 等）。在 `DGLABClient` 初始化时，根据配置加载对应的样式文件，并调用 `apply_widget_properties()` 为所有需要样式的控件设置 `type` 和 `theme` 属性。

**切换主题**：
- 点击界面上的“切换主题”按钮，弹出主题选择菜单或循环切换。
- 内部修改配置中的 `theme` 值，重新加载样式表并刷新所有控件的 `theme` 属性。

**动态设置示例**：

```cpp
QString themeStr = theme_to_mode_string(current_theme);
qApp->setProperty("theme", theme_str);
// 对每个需要应用样式的顶层控件也设置 theme
mainWindow->setProperty("theme", theme_str);
// 重新加载样式文件
load_stylesheet(theme_str);
```

**注意事项**：
- 所有自定义控件（如 `SampledWaveformWidget`）也通过 `[type="waveform"]` 选择器设置背景和圆角。
- 样式文件中的 `theme` 属性由代码动态设置，与 `type` 属性共同作用。
- 如需添加新的控件类型，请在 `apply_widget_properties()` 中为对应控件设置唯一的 `type` 字符串，并在所有样式文件中定义相应规则。
- 扩展主题的样式文件命名规则为 `style_<theme>.qcss`，其中 `<theme>` 与 `theme` 属性值保持一致（如 `charcoal_pink`）。

---

## 维护与扩展

- 新增主题时，请参照现有主题的格式创建一个新的 QSS 文件，并在此 README 中更新主题列表。
- 所有主题必须支持相同的 `type` 属性集合，以保证界面布局不因主题切换而错位。
- 建议使用 rgba 颜色值以便统一透明度控制，渐变背景使用 `qlineargradient`。
