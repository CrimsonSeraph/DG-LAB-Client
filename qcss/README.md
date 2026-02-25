# 样式文件目录（qcss）

本目录存放 Qt 样式表（QSS）文件，用于定义应用程序的视觉外观。

---

## 文件说明

### `style.qcss`：主样式表文件，定义了应用程序的默认视觉风格。

该文件使用 Qt 属性选择器（如 `[type="..."]` 和 `[mode="..."]`）来区分不同控件，实现以下样式：

- **主页面背景** (`QWidget[type="main_page"][mode="light"]`)：浅色模式下的线性渐变背景（粉蓝过渡）。
- **主页面按钮** (`QPushButton[type="main_page_btns"][mode="light"]`)：半透明白色背景，白色边框，圆角，并定义了悬停和按下状态的效果。
- **主图片标签** (`QLabel[type="main_image_label"][mode="light"]`)：半透明白色背景，居中文本，圆角。

样式基于 `type` 和 `mode` 属性进行匹配，便于后续扩展暗色模式或其他主题。
