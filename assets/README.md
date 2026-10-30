# 资源文件目录（assets）

本目录存放 DG-LAB-Client 的全部资源文件（不含 UI 设计文件）。

## 文件夹说明

| 文件夹名称 | 描述 |
| --- | --- |
| `icon` | 存放应用图标资源：源矢量 SVG、多尺寸 PNG 与 Windows 多尺寸 ICO，供窗口图标与可执行文件图标使用。 |
| `font` | 存放界面图标字体（Font Awesome Free）与其许可文本，供 QML 组件 `AppIcon` 渲染导航等处的图标。 |

> 旧界面的常规图片目录（`main_image.png` 等第三方壁纸素材）已整体移除，其在 QML 与 C++ 中均无消费者。导航栏左上角的 logo 直接复用应用图标的 128px 版本（`icon/dglab-client-128.png`），由 `DGLABClient.qrc` 的 `/icon` 前缀打进资源，因此本目录只需 `icon/`。

## 应用图标

应用图标集中在 `icon/` 目录，源矢量与各尺寸位图放在一起，便于重新导出与复用。

| 文件 | 用途 |
| --- | --- |
| `icon/dglab-client.svg` | 源矢量（1024 画布），唯一设计源，所有位图与 ico 均由它导出 |
| `icon/dglab-client-16.png` | 16px：窗口标题栏、任务栏小图标、资源管理器小图标视图 |
| `icon/dglab-client-24.png` | 24px：Windows 中等图标视图与部分 DPI 缩放档 |
| `icon/dglab-client-32.png` | 32px：任务栏与 Alt-Tab 的常用档，也是 exe 文件图标的首选层 |
| `icon/dglab-client-48.png` | 48px：资源管理器中等图标视图 |
| `icon/dglab-client-64.png` | 64px：大图标视图、QIcon 的高 DPI 补充档 |
| `icon/dglab-client-128.png` | 128px：特大图标视图、高 DPI 缩放 |
| `icon/dglab-client-256.png` | 256px：QIcon 的基准尺寸，也是 ico 的最大层 |
| `icon/dglab-client-512.png` | 512px：商店页与文档插图、打包时再缩小使用 |
| `icon/dglab-client-1024.png` | 1024px：与源矢量画布等大的母版位图 |
| `icon/dglab-client.ico` | Windows 多尺寸图标（16/24/32/48/64/128/256），作为 exe 文件图标嵌入可执行文件 |

接入方式：

- `dglab-client-256/64/32/16.png` 经 `DGLABClient.qrc` 的 `/icon` 前缀打进 Qt 资源（`:/icon/assets/icon/...`），由 `main.cpp` 组合成多尺寸 `QIcon` 设为窗口图标，标题栏、任务栏与 Alt-Tab 共用。
- `dglab-client.ico` 只交给 Windows 资源脚本 `app.rc` 编译进 `DG-LAB-Client.exe`，不进入 Qt 资源：Qt 读取 ico 依赖 imageformats 插件，运行时用 PNG 组更稳妥。

**来源声明**：本图标为本项目内的任务中原创生成，非第三方素材，无外部版权与授权限制，可随项目一同分发。图形含义：深色方形底代表设备与客户端的运行底色，居中的粉色左右对称脉冲对应 DG-LAB 设备的 A/B 双通道输出。

> **重新生成提醒**：图形若有改动，必须从 `dglab-client.svg` 同时重新导出**全部 PNG 尺寸**与**`.ico`**。只更新 PNG 会让窗口图标（PNG 组）与 exe 文件图标（ico）显示成两版不同的图；只更新 ico 则任务栏与 Alt-Tab 仍是旧图。导出后逐个核对像素尺寸，再重新构建，ico 才会被重新编入可执行文件。

## 图标字体（Font Awesome Free）

界面图标用字体方案：`AppIcon`（`src/ui/qml/components/AppIcon.qml`）通过 `FontLoader` 注册字体，再用 `Text` 渲染字符码点，颜色随主题（`Theme` 令牌）变化，不引入位图与多尺寸导出。

| 文件 | 用途 |
| --- | --- |
| `font/fa-solid-900.otf` | Font Awesome 7 Free **Solid**（排版族名 `Font Awesome 7 Free`，字重 900），导航与状态图标全部取自该字重 |
| `font/fa-regular-400.otf` | Font Awesome 7 Free **Regular**（同一族名，字重 400），免费版仅少量图标有常规字重 |
| `font/LICENSE.txt` | Font Awesome Free 许可全文（原样保留，未做任何修改） |

**来源**：Font Awesome Free 7.3.1 桌面版（`fontawesome-free-7.3.1-desktop`）的 `otfs/` 目录，原始文件名为 `Font Awesome 7 Free-Solid-900.otf` 与 `Font Awesome 7 Free-Regular-400.otf`，入库时改为 ASCII 简名，字形内容未改动（可用 md5 与原始文件比对）。

**许可**：Font Awesome Free License（见 `font/LICENSE.txt`）。其中图标为 CC BY 4.0、字体为 SIL OFL 1.1、代码为 MIT；本项目只使用字体文件（fonts），按 OFL 1.1 随附许可全文即可分发。

**加载方式**：字体经 `DGLABClient.qrc` 的 `/font` 前缀打进 Qt 资源，运行期路径为 `qrc:/font/assets/font/fa-solid-900.otf`，因此无论从构建输出目录还是安装目录启动都能加载。构建期 `assets` 整体经 `copy_directory` 复制到输出目录的既有行为不变，字体在输出目录里也有一份，但运行期只读 qrc。

> **字重陷阱**：两个字体文件的排版族名（name ID16）都是 `Font Awesome 7 Free`，仅字重不同（900 / 400）。`FontLoader.name` 对两者都返回同一个族名，因此 `AppIcon` 必须显式给 `font.weight`（900 = Solid / 400 = Regular）——只写族名会落到 Regular 字重，只有 `house`、`file-lines` 这类同时提供常规字重的图标能显示，其余 solid-only 图标会渲染成缺字方框。

**字符码点**（取自该版本 `metadata/icons.json` 的 `unicode` 字段，均已用 fontTools 在字体 cmap 中确认存在）：

| 用途             | 图标名         | 码点   |
| ---------------- | -------------- | ------ |
| 导航「主页」     | `house`        | `f015` |
| 导航「插件中心」 | `puzzle-piece` | `f12e` |
| 导航「波形设置」 | `wave-square`  | `f83e` |
| 导航「小游戏」   | `gamepad`      | `f11b` |
| 导航「日志」     | `file-lines`   | `f15c` |

> **升级提醒**：升级 Font Awesome 版本时，三个文件（两个 otf + LICENSE.txt）必须一起替换，并重新核对上表码点——官方在不同大版本间调整过个别图标的码点与字重归属。
