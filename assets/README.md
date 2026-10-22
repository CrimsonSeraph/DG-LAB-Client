# 资源文件目录（assets）

本目录存放 DG-LAB-Client 的全部资源文件（不含 UI 设计文件）。

---

## 文件夹说明

| 文件夹名称 | 描述 |
| --- | --- |
| `icon` | 存放应用图标资源：源矢量 SVG、多尺寸 PNG 与 Windows 多尺寸 ICO，供窗口图标与可执行文件图标使用。 |

> 旧界面的常规图片目录（`main_image.png` 等第三方壁纸素材）已整体移除，其在 QML 与 C++ 中均无消费者。导航栏左上角的 logo 直接复用应用图标的 128px 版本（`icon/dglab-client-128.png`），由 `DGLABClient.qrc` 的 `/icon` 前缀打进资源，因此本目录只需 `icon/`。

---

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
