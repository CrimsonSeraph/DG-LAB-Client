# 构建与测试

面向开发者：依赖、构建、测试、自动化构建、本机环境事实与交付前的验收清单。功能见 [features.md](features.md)，架构见 [architecture.md](architecture.md)，常见问题见 [faq.md](faq.md)。

## 系统依赖

- **C++ 编译器**: 支持 C++20 标准（如 GCC 10+、Clang 12+、MSVC 2022）
- **Qt 6**: Core、Gui、Network、Qml、Quick、QuickControls2、QuickDialogs2、WebSockets、Bluetooth、Widgets（系统托盘菜单）、Concurrent
- **Python**（可选）: 仅 GSI 插件的 `PathFinder.py`（查找 CS 游戏目录）需要

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

## 单元测试（可选构建）

单元测试是裸断言测试，由 CMake 开关 `DGLAB_BUILD_TESTS` 控制，**默认 OFF**：普通构建与发布构建都不编译测试目标。测试程序落在构建目录的 `tests/` 下（多配置生成器再追加一层配置子目录，如 `build/tests/Release`），既不与应用、`module/` 插件混在同一个输出目录，也不在 `install()` 清单里，因此不会进安装包。

### 默认构建（不含测试）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt
cmake --build build --config Release
```

输出目录里只有 `DG-LAB-Client`、插件与 Qt 运行时；此时 `ctest` 无测试可跑，报 `No tests were found`。

### 启用测试的构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt -DDGLAB_BUILD_TESTS=ON
cmake --build build --config Release
ctest -C Release --output-on-failure --test-dir build
```

当前 16 个目标：`event_engine` / `rule_graph_eval` / `event_engine_module` / `event_merge` / `event_wave` / `opossum_frames` / `device_link` / `paw_device` / `civet_device` / `input_link` / `model_recognition` / `wave_library` / `wave_import_export` / `wave_zip` / `wave_tag` / `config_reload`。`input_link` 覆盖输入设备多实例的实例模块名生成与同后四位消歧、连接注册 / 断开注销（数值模块、内置事件声明与引擎运行期状态三者同步）与旧配置迁移。测试程序不与 windeployqt 部署过的输出目录同处一地，Windows 上加载 Qt DLL 所需的环境变量由 CMake 从 `Qt6::Core` 的导入位置推导后写进 `ctest` 环境，不需要手工配 PATH；三个平台的 CI 工作流都带 `-DDGLAB_BUILD_TESTS=ON` 构建并执行 `ctest`，测试失败即工作流失败。

## 验收清单

每次交付按此核对:

1. 构建 `cmake --build build-agent --config Release` 0 error；带 `-DDGLAB_BUILD_TESTS=ON` 配置时 `ctest -C Release --test-dir build-agent` 16/16 全绿。
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
