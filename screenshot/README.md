# 截图目录 (screenshot)

本目录包含 DG-LAB-Client 的主要界面截图，按功能模块分为三个子目录：`others/`、`pages/` 和 `themes/`。您可以直接点击下方图片链接查看，或在文件管理器中打开对应文件。

---

## 📁 目录结构

```
screenshot/
├── others/               # 辅助功能/对话框截图
│   ├── Console.png       # Windows 调试控制台
│   ├── IpSelector.png    # IP 选择/黑白名单编辑对话框
│   ├── ThemeSelectorDialog.png  # 主题选择对话框（卡片视图）
│   └── ValueModeDelegate.png    # 规则值模式公式构建器
├── pages/                # 主界面页面截图
│   ├── main_page.png     # 首页（连接/通道控制/波形）
│   ├── config_page.png   # 配置页（规则表格编辑）
│   └── rule_page.png     # 规则文件管理页
└── themes/               # 所有内置主题的预览截图
    ├── light.png
    ├── night.png
    ├── charcoal_pink.png
    ├── deepsea_cream.png
    ├── vine_purple_tea_green.png
    ├── offwhite_camellia.png
    ├── dark_blue_clear_blue.png
    ├── klein_yellow.png
    ├── mars_green_rose.png
    ├── hermes_orange_navy.png
    ├── tiffany_blue_cheese.png
    ├── china_red_yellow.png
    ├── vandyke_brown_khaki.png
    └── prussian_blue_fog.png
```

> **说明**：主题截图展示了同一界面（例如首页）在不同主题下的配色效果，完整主题共 14 种。

---

## 🖼️ Others – 辅助功能/对话框

| 截图 | 说明 |
|------|------|
| ![Console](others/Console.png) | **Windows 调试控制台** <br>当 `app.debug = true` 时自动创建，显示详细日志输出。 |
| ![IpSelector](others/IpSelector.png) | **IP 选择器对话框** <br>支持编辑黑白名单、手动选择可用 IP。 |
| ![ThemeSelectorDialog](others/ThemeSelectorDialog.png) | **主题选择对话框** <br>网格卡片形式展示所有主题，点击即切换。 |
| ![ValueModeDelegate](others/ValueModeDelegate.png) | **规则值模式公式构建器** <br>双击表格“值模式”单元格弹出，支持 `{}` 占位符和 `+-*/()` 表达式。 |

---

## 📄 Pages – 主界面页面

| 截图 | 说明 |
|------|------|
| ![main_page](pages/main_page.png) | **首页** <br>包含连接信息、A/B 通道强度控制、实时波形显示区域。 |
| ![config_page](pages/config_page.png) | **配置页** <br>规则表格展示，支持添加/编辑/删除规则，修改规则文件。 |
| ![rule_page](pages/rule_page.png) | **规则文件管理页** <br>切换/新建/删除/保存规则文件，管理规则文件列表。 |

---

## 🎨 Themes – 主题预览

所有主题均以上述首页（main_page）为例，展示不同的配色方案。

| 主题名称 | 英文模式名 | 预览图 |
|---------|-----------|--------|
| 浅色模式 | light | ![light](themes/light.png) |
| 深色模式 | night | ![night](themes/night.png) |
| 炭黑甜粉 | charcoal_pink | ![charcoal_pink](themes/charcoal_pink.png) |
| 深海奶白 | deepsea_cream | ![deepsea_cream](themes/deepsea_cream.png) |
| 藤紫钛绿 | vine_purple_tea_green | ![vine_purple_tea_green](themes/vine_purple_tea_green.png) |
| 无白茶花 | offwhite_camellia | ![offwhite_camellia](themes/offwhite_camellia.png) |
| 捣蓝清水 | dark_blue_clear_blue | ![dark_blue_clear_blue](themes/dark_blue_clear_blue.png) |
| 克莱因黄 | klein_yellow | ![klein_yellow](themes/klein_yellow.png) |
| 马尔斯玫瑰 | mars_green_rose | ![mars_green_rose](themes/mars_green_rose.png) |
| 爱马仕深蓝 | hermes_orange_navy | ![hermes_orange_navy](themes/hermes_orange_navy.png) |
| 蒂芙尼奶酪 | tiffany_blue_cheese | ![tiffany_blue_cheese](themes/tiffany_blue_cheese.png) |
| 中国红黄 | china_red_yellow | ![china_red_yellow](themes/china_red_yellow.png) |
| 凡戴克棕卡其 | vandyke_brown_khaki | ![vandyke_brown_khaki](themes/vandyke_brown_khaki.png) |
| 普鲁士雾灰 | prussian_blue_fog | ![prussian_blue_fog](themes/prussian_blue_fog.png) |

> **注意**：如果部分主题截图尚未添加，对应位置将显示占位符。请确保 `themes/` 目录下包含全部 14 张图片，且文件名与上表一致。

---

## 📝 使用说明

- 您可以在项目主 `README.md` 的“截图”章节中引用本文件，或直接在此目录下浏览图片。
- 截图基于 Windows 11 + Qt 6.5.0 默认字体（微软雅黑 9pt）生成，实际显示可能因操作系统和 Qt 版本略有差异。
- 所有截图均在 1920×1080 分辨率下截取，未作后期处理。

---

## 🔧 维护

如需更新截图，请保持以下约定：
- 命名使用小写字母和下划线（snake_case）。
- PNG 格式，建议无压缩或低压缩，保证文字清晰。
- 同功能区域的新截图请放入对应的子目录（`others/`、`pages/`、`themes/`）。
- 更新后请同步修改本 README 中的文件名和说明。

--- 

*最后更新：2026-05-03*
