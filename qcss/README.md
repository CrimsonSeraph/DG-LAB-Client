# 样式文件目录（qcss）

本目录存放 Qt 样式表（QSS）文件，用于定义应用程序的视觉外观。

---

## 文件说明

本目录包含两个样式文件，分别支持浅色模式和暗夜模式。**v0.3.0 对样式表进行了全面重构**，引入了 `type` 和 `mode` 属性选择器，实现更精细的控件分类和主题切换。

### `style_light.qcss`：浅色模式样式表

该文件定义了应用程序在浅色模式下的视觉风格，使用 Qt 属性选择器（如 `[type="..."]` 和 `[mode="light"]`）来区分不同控件。主要控件类型包括：

| 属性值 | 控件类型 | 说明 |
| - | - | - |
| `main_page` | 主窗口背景 | 线性渐变背景（浅粉到浅蓝） |
| `glassmorphism` | 玻璃拟态容器 | 半透明白色背景，白色边框，圆角 |
| `nav_btn` | 导航按钮 | 深蓝渐变，白色文字，圆角 |
| `action_btn` | 操作按钮 | 浅蓝渐变，深色文字，细边框 |
| `title` | 标题文字 | 大号加粗，深灰色，居中 |
| `label` | 普通标签 | 常规字体，深灰色，左对齐 |
| `image` | 图片标签 | 半透背景，圆角，文字居中 |
| `input` | 输入框 | 白色背景，灰色边框，焦点变色 |
| `debug_log` | 日志文本框 | 深灰背景，等宽字体 |
| `scrollarea` | 滚动区域 | 透明背景，无边框 |
| `stacked` | 堆叠窗口 | 透明背景 |
| `sub_panel` | 子面板 | 完全透明，用于布局 |

示例样式片段：
```css
QPushButton[type="nav_btn"][mode="light"] {
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
QPushButton[type="nav_btn"][mode="light"]:hover {
    background: qlineargradient(...);
}
```

### `style_night.qcss`：暗夜模式样式表

暗夜模式使用相同的 `type` 属性，但 `mode="night"`，配色方案调整为深色背景和浅色文字。示例：
```css
QPushButton[type="nav_btn"][mode="night"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 rgba(58,90,126,1),
                                stop:1 rgba(42,74,110,1));
    color: #e0e0e0;
}
```

---

## 使用说明

应用程序通过 `AppConfig` 读取 `app.ui.is_light_mode` 配置（默认 true），并在 `DGLABClient` 初始化时调用 `load_stylesheet()` 加载对应的样式文件。同时，`apply_widget_properties()` 会为所有需要样式的控件设置 `type` 属性，样式表中的选择器基于这些属性进行匹配。

**切换主题**：点击界面上的“切换主题”按钮会调用 `change_theme()`，内部修改配置并重新加载样式表。

**注意事项**：
- 所有自定义控件（如 `SampledWaveformWidget`）也通过 `[type="waveform"]` 选择器设置背景和圆角。
- 样式文件中的 `mode` 属性由 `this->setProperty("mode", is_light_mode_ ? "light" : "night")` 动态设置，与 `type` 属性共同作用。
- 如需添加新的控件类型，请在 `apply_widget_properties()` 中为对应控件设置唯一的 `type` 字符串，并在两个样式文件中定义相应规则。
