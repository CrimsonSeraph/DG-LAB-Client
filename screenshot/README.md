# 截图目录 (screenshot)

本目录存放 DG-LAB-Client 当前 QML 界面的文档用截图，全部位于 `pages/`，共四张，与左侧导航的四个页面一一对应。

---

## 目录结构

```
screenshot/
├── pages/                # 当前界面四页
│   ├── main_page.png     # 首页（连接、A/B 通道、波形）
│   ├── config_page.png   # 配置页（连接、规则表、日志）
│   ├── module_page.png   # 模块页（插件与数值卡片）
│   └── about_page.png    # 关于页（版本、链接、许可信息）
└── README.md             # 本文件
```

---

## 页面说明

| 截图 | 页面 | 内容 |
| --- | --- | --- |
| ![main_page](pages/main_page.png) | 首页 | 页头、配对二维码与连接状态、A/B 通道卡片（模块 / 规则 / 波形）、状态栏常驻摘要。 |
| ![config_page](pages/config_page.png) | 配置页 | 连接与主题设置、规则文件切换与规则表、日志面板与导出入口。 |
| ![module_page](pages/module_page.png) | 模块页 | 插件与静态模块卡片网格，卡片显示加载状态与最小查询周期，点击可查看并设置数值周期。 |
| ![about_page](pages/about_page.png) | 关于页 | 应用名称与版本（版本号来自 `config/main.json`）、项目链接与许可说明。 |

---

## 规格

- 分辨率统一为 **1618×1047**：125% DPI 缩放下的 1280×800 窗口（含窗口边框）按整数物理像素截取，未缩放、未裁剪、未后期处理。
- PNG 无损格式，文字清晰；不使用阴影装饰或拼图。
- 截图环境：Windows 11 + Qt 6 Release 构建，默认主题与默认字体。

---

## 如何重新生成

1. 以 Release 构建并启动应用（`cd config` 等同级目录就位后运行 `DG-LAB-Client.exe`）。
2. 保持窗口尺寸为 1280×800、系统缩放 125%，按左侧导航依次进入首页、配置、模块、关于四个页面。
3. 逐页截图，并按 `pages/main_page.png`、`config_page.png`、`module_page.png`、`about_page.png` 原名覆盖。
4. 四张图应来自同一次构建、同一主题、同一窗口尺寸，否则页面之间的间距与徽章配色会对不上。
5. 重新生成后同步更新本文件与主 [README](../README.md) 的截图章节。

---

## 已移除的历史截图

- `others/`（`Console`、`IpSelector`、`ThemeSelectorDialog`、`ValueModeDelegate`）与 `themes/`（14 套主题预览）已在 v2.3.0 一并移除。
- 原因：这些截图拍的是已废弃的 Widgets 界面（QSS 样式表、旧对话框与旧主题选择器），与当前 QML 界面（`Theme` 单例 + 设计令牌）不是同一套渲染结果，留着会误导使用者。
- 旧的 `pages/rule_page.png` 也一并删除：规则编辑已并入配置页，不再存在独立的规则文件管理页。
