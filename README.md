# DG-LAB-Client

一个基于 Qt 的桌面客户端，用于与 DG-Lab 服务进行 WebSocket 通信，并通过 Python 脚本扩展功能。  
项目采用 C++20 编写，集成了配置管理、多级日志、Python 解释器（支持线程池）、异步任务等特性。

---

## 一、项目简介

DG-LAB-Client 是一个为 DG-Lab（地牢实验室）设计的客户端工具，它通过 WebSocket 与服务端通信，支持：

- 同步/异步调用 Python 脚本实现自定义逻辑；
- 多配置文件管理（主配置、系统配置、用户配置），支持优先级覆盖；
- 调试控制台与分级日志；
- 通过 Qt 设计的图形化界面；
- 线程池执行 Python 任务，避免阻塞 UI。

> **注意**：项目尚在开发中，部分功能仍在完善。

---

## 二、功能特性

- **Python 脚本集成**  
  通过 `PyExecutor` / `PyThreadPoolExecutor` 调用 Python 模块中的类和方法，支持同步、异步、线程池执行。`PyExecutorManager` 统一管理多个执行器。

- **配置系统**  
  采用 `MultiConfigManager` 管理多个 JSON 配置文件，支持优先级覆盖、热重载、配置变更监听。配置项通过 `ConfigValue<T>` 或 `ConfigObject<T>` 包装，提供类型安全访问和缓存。

- **日志与调试**  
  `DebugLog` 提供模块化日志等级控制，可输出到控制台、Qt 界面等不同的 `LogSink`；通过 `Console` 类可在 Windows 上创建调试控制台。

- **WebSocket 通信**  
  附带的 `WebSocketCore.py` 用于与 DG-Lab 服务的基本 WebSocket 交互（连接、心跳、绑定、控制命令），并提供了同步包装方法供 C++ 调用。

- **跨平台构建**  
  基于 CMake，支持 Windows、Linux、macOS 等平台（需自行适配 Qt 和 Python 路径与附加包文件等内容）。

---

## 三、依赖项

### 1. 系统依赖
- **C++ 编译器**：支持 C++20 标准（如 GCC 10+、Clang 12+、MSVC 2022）
- **Qt**：5.15 或 6.x（Core、Gui、Widgets）
- **Python**：3.9 或更高版本（开发库及解释器）

### 2. 第三方库
项目使用 CMake FetchContent 在配置时自动下载以下库，**无需手动操作**：
- **[pybind11](https://github.com/pybind/pybind11)** (版本 2.13.6)  
  用于 C++ 与 Python 交互。
- **[nlohmann/json](https://github.com/nlohmann/json)** (版本 3.12.0)  
  用于 JSON 解析。

**注意：** 若网络环境无法访问 GitHub，可能需要手动下载这些库并 **放置/指定** 位置：
- pybind11：通过 `cmake -DFETCHCONTENT_SOURCE_DIR_PYBIND11=/path/to/your/pybind11` 指定路径
- nlohmann/json：构建目录下的 `include/nlohmann/json.hpp`

### 3. Python 包依赖
脚本 `python/WebSocketCore.py` 使用了以下 Python 库，请确保已安装（或由 CI 自动处理）：
- `websockets` (建议版本 10.0 或更高)  
  安装命令：`pip install websockets`

---

## 四、快速开始

### 1. 获取源码

```bash
git clone https://github.com/yourusername/DG-LAB-Client.git
cd DG-LAB-Client
```

### 2. 准备第三方库

CMake 会在配置时自动下载 pybind11 和 nlohmann/json（需要良好的 GitHub 连接）。如遇网络问题，可手动下载并放置到相应位置。

### 3. 安装 Python 依赖

```bash
pip install websockets
```

### 4. 配置 CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt -DPython_ROOT_DIR=/path/to/python
```

- 对于 Qt6，CMake 通常能自动找到；若使用 Qt5，请确保 `Qt5` 包可用。
- 可通过 `-DPYTHON_PACKAGES_DIR=path/to/site-packages` 指定要打包的第三方 Python 包目录（可选）。

### 5. 编译

```bash
cmake --build . --config Release
```

### 6. 安装到临时目录（可选，用于测试打包结果）

```bash
cmake --install . --prefix install_root
```

### 7. 打包（生成安装包）

```bash
cpack
```

生成的安装包将位于 build/ 目录下（根据平台不同，可能是 .exe、.dmg、.deb 等）。

---

## 五、自动化构建

本项目已配置 GitHub Actions，每次推送到 `main` 分支或创建以 `v` 开头的标签时，会自动在 Ubuntu、Windows 和 macOS 上构建并打包，生成对应平台的安装包：

- **Windows**: NSIS 安装程序 (`.exe`) 和 ZIP 压缩包
- **macOS**: DMG 磁盘映像和 TGZ 压缩包
- **Linux**: DEB、RPM 包以及 AppImage（自包含可执行文件）

你可以在 GitHub 仓库的 **Actions** 页面下载最新构建产物，或从 **Releases** 页面获取正式发布的版本。

---

## 六、使用说明

### 1. 配置文件

系统包含三个配置文件，按优先级从低到高依次为：

- `main.json`：主配置，优先级 0
- `system.json`：系统配置，优先级 1
- `user.json`：用户配置，优先级 2

高优先级的配置项会覆盖低优先级的同路径配置。  
**注意：** 即使有覆盖逻辑，仍不建议在不同配置文件中定义相同属性  
配置文件中 `__priority` 字段用于定义优先级，不可删除。

示例 `main.json`：

```json
{
    "__priority": 0,
    "app": {
        "name": "DG-LAB-Client",
        "version": "0.2.0",
        "debug": false,
        "log": {
            "console_level": 0,
            "only_type_info": false,
            "ui_log_level": 0
        }
    },
    "python": {
        "path": "python",
        "packages_path": "python/Lib/site-packages"
    },
    "version": "1.0",
    "DGLABClient": "DG-LAB-Client"
}
```
**注意：** 其中 `"version"` 与 `"DGLABClient"` 为检查字段，内容随意，但 **请勿删除或修改** 此字段。

### 2. Python 脚本

默认 Python 脚本存放于可执行文件所在目录的 `python/` 文件夹中。  
脚本 `WebSocketCore.py` 提供了 `DGLabClient` 类，可通过 C++ 调用其方法进行 WebSocket 连接与控制。

### 3. 日志

日志等级通过配置文件 `app.log.console_level`、`app.log.ui_log_level` 控制（数值含义见下表）。您也可以在代码中使用 `LOG_MODULE` 宏输出日志：

```cpp
LOG_MODULE("MyModule", "my_function", LOG_INFO, "This is a log message.");
```

日志等级枚举：
- 0：DEBUG
- 1：INFO
- 2：WARN
- 3：ERROR
- 4：NONE

可通过 `DebugLog::set_log_sink_level("qt_ui", level)` 动态调整 UI 日志显示级别。

### 4. 调试控制台

在 Windows 上，如果配置文件中的 `app.debug` 为 `true`，程序启动时会自动创建一个调试控制台，用于显示详细的日志输出。
**注意：** 该控制台使用 `#include <windows.h>` 仅在 Windows 平台上可用，并且需要在配置文件中启用调试模式。
当然，应用中首页也会输出日志到 Qt 界面，您可以根据需要选择查看。

---

## 七、本地 WebSocket 中转服务部署

本项目作为客户端，需要连接一个本地的 WebSocket 中转服务才能与 DG-Lab App 进行通信。该中转服务基于 Node.js 开发，您需要先在电脑上启动它。

### 1. 服务说明

此服务（默认文件名为 `websocketNode.js`）扮演着中央中转站的角色：
1.  它接收本客户端（DG-LAB-Client）的连接。
2.  它接收 DG-Lab 官方手机 App 的连接（官方似乎只有 3.x 支持 websocket）。
3.  它负责在两者之间中继消息（如绑定指令、强度控制、心跳包），使电脑和手机能通过 WebSocket 进行数据交换。

### 2. 环境准备

-   **安装 Node.js**：确保您的系统已安装 [Node.js](https://nodejs.org/) (建议版本 12.x 或更高)。
-   **获取服务脚本**：从 [DG-LAB-OPENSOURCE 仓库](https://github.com/DG-LAB-OPENSOURCE/DG-LAB-OPENSOURCE/tree/main/socket/BackEnd(Node)) 下载 `websocketNode.js` 文件，并将其保存到您电脑上的一个专用文件夹中（例如 `DGLab-server/`）。

### 3. 安装与启动

打开终端（命令提示符或 PowerShell），进入存放 `websocketNode.js` 的文件夹，执行以下步骤：

**步骤一：安装依赖**
该服务通常依赖 `ws` 库（WebSocket 实现）。运行以下命令安装依赖：
```bash
npm install ws uuid
```

**步骤二：启动服务**
在同一个目录下，运行以下命令启动服务：
```bash
node websocketNode.js
```
**端口设置**：服务启动后，控制台不会输出任何内容！**请仔查看`websocketNode.js`**，通常默认端口通常是 `9999`，具体请以您下载的脚本中定义的端口为准。

**成功标志**：如果看到类似 `新的 WebSocket 连接已建立，标识符为:xxx` 的提示，则说明服务已成功运行。

**注意：**[官方仓库](https://github.com/DG-LAB-OPENSOURCE/DG-LAB-OPENSOURCE/tree/main/socket/BackEnd(Node)) 提供了**启动服务的方法**与**相关调试信息开关方式**，详细请参考官方仓库信息

### 4. 配置与连接本客户端

1.  保持 Node.js 服务在后台运行（终端窗口不要关闭）。
2.  启动本客户端（DG-LAB-Client）。
3.  客户端会根据配置文件加载 Python 脚本，并在连接成功后生成包含服务地址和 `clientId` 的二维码内容（通过 `generate_qr_content` 方法）。

### 5. 手机 App 连接

1.  确保您的手机与电脑连接在**同一个局域网**下。
2.  打开 DG-Lab 官方手机 App（官方似乎只有 3.x 支持 websocket）。
3.  使用 App 内的扫一扫功能，扫描本客户端生成的二维码。
4.  扫描后，手机 App 将自动连接到您电脑上运行的中转服务，并与本客户端建立绑定关系。后续的强度调节、波形发送等操作即可进行。

---

## 八、项目结构

```
DG-LAB-Client/
├── CMakeLists.txt              # 主构建文件
├── qt.cmake                    # Qt 相关设置
├── include/                    # 公共头文件
│   ├── AppConfig.h
│   ├── AppConfig_impl.hpp     # 模板实现
│   ├── ConfigManager.h
│   ├── ConfigStructs.h
│   ├── Console.h
│   ├── DebugLog.h
│   ├── DefaultConfigs.h
│   ├── DGLABClient.h
│   ├── MultiConfigManager.h
│   ├── MultiConfigManager_impl.hpp
│   ├── PyExecutor.h
│   ├── PyExecutor_impl.hpp
│   ├── PyExecutorManager.h
│   ├── PyExecutorManager_impl.hpp
│   ├── PyThreadPoolExecutor.h
│   └── PyThreadPoolExecutor_impl.hpp
├── src/                        # 源文件
│   ├── AppConfig.cpp
│   ├── ConfigManager.cpp
│   ├── ConfigStructs.cpp
│   ├── Console.cpp
│   ├── DebugLog.cpp
│   ├── DefaultConfigs.cpp
│   ├── DGLABClient.cpp
│   ├── MultiConfigManager.cpp
│   ├── PyExecutor.cpp
│   ├── PyExecutorManager.cpp
│   └── PyThreadPoolExecutor.cpp
├── python/                     # Python 脚本
│   └── WebSocketCore.py
├── config/                     # 默认配置文件
│   ├── main.json
│   ├── system.json
│   └── user.json
├── assets/                     # 资源文件（图标、UI 图片等）
│   └── normal_image/
│       └── main_image.png
├── qcss/                       # Qt 样式表
│   └── style.qcss
├── DGLABClient.qrc              # Qt 资源文件
├── LICENSE.txt                  # 许可证文件
├── README.md                    # 项目说明文档
├── .editorconfig                # 编辑器配置
├── .gitattributes               # Git 属性配置
└── .gitignore                   # Git 忽略规则
```

---

## 九、截图

（暂时留空）

---

## 十、贡献指南

欢迎提交 Issue 和 Pull Request。在贡献前请确保：

- 代码遵循现有风格（缩进 4 空格，使用 `#pragma once`，命名规范）。
- 请务必使用 UTF-8 without BOM 编码提交代码。
- 添加或修改功能时更新相关文档（如 README.md）。
- 确保本地测试通过（编译通过，功能正常）。
- 对于较大的改动，请先开 Issue 讨论。

---

## 十一、许可证

本项目基于 MIT 许可证开源，详情请参阅 [LICENSE.txt](LICENSE.txt) 文件。

---

## 十二、联系方式

- 作者：[CrimsonSeraph]
- BiliBili：[浪天幽影(UID：1741002917)](https://space.bilibili.com/1741002917?spm_id_from=333.1007.0.0)
- X：[𝒞𝓇𝒾𝓂𝓈𝑜𝓃𝒮𝑒𝓇𝒶𝓅𝒽✟升天✟(@CrimSeraph_QwQ)](https://x.com/CrimSeraph_QwQ)
- 项目主页：[https://github.com/CrimsonSeraph/DG-LAB-Client](https://github.com/CrimsonSeraph/DG-LAB-Client)
