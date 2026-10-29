# 构建与测试

面向开发者：依赖、构建、测试、自动化构建、本机环境事实与交付前的验收清单。功能见 [features.md](features.md)，架构见 [architecture.md](architecture.md)，常见问题见 [faq.md](faq.md)。

## 系统依赖

- **C++ 编译器**: 支持 C++20 标准（如 GCC 10+、Clang 12+、MSVC 2022）
- **Qt 6**: Core、Gui、Network、Qml、Quick、QuickControls2、QuickDialogs2、WebSockets、Bluetooth、Widgets（系统托盘菜单）、Concurrent
- **Python**（可选）: 仅 GSI 插件的 `PathFinder.py`（查找 CS 游戏目录）需要，不再用于 WebSocket 通信

> 推荐使用 VS2022/2026（MSVC）进行 Windows 开发，Linux/macOS 可使用 GCC 或 Clang。👉 遇到依赖问题？请查看 [常见问题 - 编译与运行](faq.md#编译与运行)

## 第三方库

- **[nlohmann/json](https://github.com/nlohmann/json)** (版本 3.12.0) 用于 JSON 解析。CMake 会在配置时自动从 GitHub 下载单头文件到构建目录 `/include/nlohmann/`

## Python 包依赖

**不再需要**。原先的 WebSocket 通信与二维码生成已改为应用内实现（`DglabRelayServer` + 内嵌 `qrcodegen`），无需安装 `websockets` / `qrcode`。仅在使用 CS2 GSI 插件时需要系统中有 Python 解释器运行 `PathFinder.py`。

## 快速开始

### 1. 获取源码

```bash
git clone https://github.com/yourusername/DG-LAB-Client.git
cd DG-LAB-Client
```

### 2. 配置 CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt -DPython_ROOT_DIR=/path/to/python
```

- 对于 Qt6，CMake 通常能自动找到；若使用 Qt5，请确保 `Qt5` 包可用。
- 可通过 `-DPYTHON_PACKAGES_DIR=path/to/site-packages` 指定要打包的第三方 Python 包目录（可选，供 CI 使用）。

> 👉 配置或编译失败？请查看 [常见问题 - 编译与运行](faq.md#编译与运行)

### 3. 编译

```bash
cmake --build . --config Release
```

### 4. 运行

编译完成后，可执行文件位于 `build/Release`（Windows）或 `build`（Linux/macOS）目录下。运行时需要确保以下目录与可执行文件同级:

- `config/`: 包含 `main.json`、`system.json`、`user.json`、规则子目录（`config/rules/`）与波形库目录（`config/waves/`，官方内置波形的只读资源在 `assets/waves/`）
- `python/`: 可选，仅 `PathFinder.py`（GSI 插件查找游戏目录用）
- `assets/`: 包含资源文件（如图片）
- Qt 运行时（DLL / 平台插件 / QML 模块）由构建后自动 `windeployqt` 部署到可执行文件目录

CMake 的 `POST_BUILD` 命令会自动复制这些目录到输出目录。

### 5. 打包（生成安装包）

```bash
cpack
```

生成的安装包将位于 `build/` 目录下（根据平台不同，可能是 .exe、.dmg、.deb 等）。

## 自动化构建

- **跨平台构建** 基于 CMake，支持 Windows、Linux、macOS 等平台，并通过 GitHub Actions 自动构建和打包。

本项目已配置 GitHub Actions，每次推送到 `main` 分支或创建以 `v` 开头的标签时，会自动在 Ubuntu、Windows 和 macOS 上构建并打包，生成对应平台的安装包:

- **Windows**: NSIS 安装程序 (`.exe`) 和 ZIP 压缩包
- **macOS**: DMG 磁盘映像和 TGZ 压缩包
- **Linux**: DEB、RPM 包以及 AppImage（自包含可执行文件）

你可以在 GitHub 仓库的 **Actions** 页面下载最新构建产物，或从 **Releases** 页面获取正式发布的版本。

## 测试

单元测试是裸断言测试，只链接 `Qt::Core`，通过 CTest 运行:

```bash
cd build && ctest -C Release --output-on-failure
```

当前 9 个目标全绿：`event_engine` / `rule_graph_eval` / `event_engine_module` / `event_merge` / `wave_library` / `wave_import_export` / `wave_zip` / `wave_tag` / `event_wave`。

## 开发环境事实

Windows 本机实测口径，换环境时逐条重新确认:

- Qt 6.9.3 `msvc2022_64`，Visual Studio 18 2026 生成器，MSVC。
- 构建目录是 `build-agent`（临时构建树，未入库）。构建命令必须带 TMP 覆盖，否则 MSBuild 因临时目录受限失败:

```bash
cd /f/Projects/DG-LAB-Client && export TMP="$PWD/build-agent/tmp" TEMP="$PWD/build-agent/tmp" TMPDIR="$PWD/build-agent/tmp" && cmake --build build-agent --config Release --parallel 8
```

- 前台调用有 300 s 上限；长构建用 `run_in_background: true` + `job_output(wait: true)`。
- **构建前必须清实例**，否则链接阶段占用 exe:

```bash
powershell -NoProfile -Command "Get-Process -Name `DG-LAB-Client` -ErrorAction SilentlyContinue | Stop-Process -Force"
```

- 本机 125% 缩放：截图脚本**必须 DPI 感知**（`SetProcessDpiAwarenessContext(-4)`），否则 `GetWindowRect` 给出 ×0.8 的假尺寸，会把正确布局误报成裁切；截图坐标要用**屏幕绝对坐标**（`SetCursorPos` 不收窗口相对偏移）。
- QML 运行期错误的唯一可靠抓法:

```bash
cd build-agent/Release && (QT_FORCE_STDERR_LOGGING=1 QT_ASSUME_STDERR_HAS_CONSOLE=1 ./DG-LAB-Client.exe > out.txt 2> err.txt &) ; sleep 8
```

  验收要求 `err.txt` 0 字节。日志文件开关：config 里 `app.log.file_enabled=true` 会写 `<exe>/log/dglab-YYYYMMDD.log`。
- **界面取证禁用 PostMessage 注入**：本机 PostMessage 注入的鼠标与键盘消息对该 Qt 进程无效（返回成功但界面无反应），历史上曾据此得出「已点击验证」的假结论。界面取证一律改用 **UI Automation**（InvokePattern / ValuePattern），或直接函数级单测 + PrintWindow 静态截图，并在报告里区分「界面验证」与「函数级验证」。
- **波形写入没有回执**：郊狼的 B0 采用 WriteWithoutResponse，不回报 `characteristicWritten`，不能用写入回调取证下发成功；波形进度只能按设备实际写入的帧数推进（`advance_event_wave()` 即按此实现）。
- 两个构建者不要共用一个构建目录：同一 `build-agent` 被两个进程构建会互相抢 exe（LNK1104）。

## 验收清单

每次交付按此核对:

1. 构建 `cmake --build build-agent --config Release` 0 error；`ctest -C Release` 全绿。
2. 启动 `QT_FORCE_STDERR_LOGGING=1 QT_ASSUME_STDERR_HAS_CONSOLE=1`，`err.txt` 与 `out.txt` 均 0 字节。
3. DPI 感知截图与需求 / 参考截图对照；界面无颜色字面量（只用 `Theme` 令牌）。
4. 文档同步项已更新（接口、字段、objectName、节点表）。
5. 数据落盘位置正确（`user.json` 的 `app.channels.*`、`app.plugins.<模块>.events.*`、`config/rules/*.json`）。
6. 按规范提交，并在回复里给出提交哈希与一句话摘要。

## 相关文档

- [architecture.md](architecture.md)：分层、数据流与开发陷阱
- [features.md](features.md)：功能与使用说明
- [faq.md](faq.md)：编译与运行相关常见问题
- [plugin-development.md](plugin-development.md)：插件构建示例
- [../CONTRIBUTING.md](../CONTRIBUTING.md)：代码风格、文档规范与提交格式
