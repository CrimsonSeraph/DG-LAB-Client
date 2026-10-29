# 截图

图片本身放在仓库根目录的 @@screenshot/@@ 下，@@docs/@@ 只写文字说明，不放图片。

## 与 screenshot/README.md 的分工

- [../screenshot/README.md](../screenshot/README.md) 是**唯一权威**：目录树、文件名清单、分辨率规格与重新生成步骤都写在那里。
- 本文负责**选图与引用口径**：为什么只保留这三张、历史截图为何移除、文档里怎么引用它们，避免两处各写一套而漂移。

## 当前界面（三张）

| 截图 | 页面 | 内容 |
| --- | --- | --- |
| ![main_page](../screenshot/pages/main_page.png) | 首页 | 页头、配对二维码与连接状态、A/B 通道卡片（模块 / 规则 / 波形）、状态栏常驻摘要。 |
| ![config_page](../screenshot/pages/config_page.png) | 配置页 | 连接与主题设置、规则文件切换与规则表、日志面板与导出入口。 |
| ![about_page](../screenshot/pages/about_page.png) | 关于页 | 应用名称与版本（版本号来自 @@config/main.json@@）、项目链接与许可说明。 |

> 配置页与关于页的信息在后续版本中拆到了独立页面（日志页、波形设置页、更新对话框），上表描述的是截图拍摄时的界面；重新生成截图时一并更新它。

## 历史截图的移除

- @@others/@@（@@Console@@、@@IpSelector@@、@@ThemeSelectorDialog@@、@@ValueModeDelegate@@）与 @@themes/@@（14 套主题预览）已在 v2.3.0 一并移除。
- 原因：这些截图拍的是已废弃的 Widgets 界面（QSS 样式表、旧对话框与旧主题选择器），与当前 QML 界面（@@Theme@@ 单例 + 设计令牌）不是同一套渲染结果，留着会误导使用者。
- 旧的 @@pages/rule_page.png@@ 也一并删除：规则编辑已并入配置页，不再存在独立的规则文件管理页。

## 拍图纪律

- 统一分辨率 @@1618×1047@@：125% 缩放下的 1280×800 窗口按整数物理像素截取，不缩放、不裁剪、不做后期处理。
- 三张图必须来自同一次构建、同一主题、同一窗口尺寸，否则页面之间的间距与徽章配色会对不上。
- 截图脚本必须 DPI 感知，坐标用屏幕绝对坐标；界面取证不要用 PostMessage 注入（对本进程无效）。环境细节见 [build.md](build.md)。
- 重新生成后同步更新 [../screenshot/README.md](../screenshot/README.md) 与本文表格。

## 相关文档

- [features.md](features.md)：各页面的功能说明
- [theme.md](theme.md)：主题令牌与预设
- [build.md](build.md)：构建、DPI 与截图环境事实
- [../screenshot/README.md](../screenshot/README.md)：截图目录与重生成步骤
