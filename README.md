# DG-LAB-Client

一个基于 Qt 的桌面客户端，用于与 DG-Lab 服务进行 WebSocket 通信，并通过 Python 脚本扩展功能。  
项目采用 C++20 编写，集成了配置管理、多级日志、Python 解释器（支持线程池）等特性。

---

## 项目简介

DG-LAB-Client 是一个为 DG-Lab（地牢实验室）设计的客户端工具，它通过 WebSocket 与服务端通信，支持：

- 同步/异步调用 Python 脚本实现自定义逻辑；
- 多配置文件管理（主配置、系统配置、用户配置），支持优先级覆盖；
- 调试控制台与分级日志；
- 通过 Qt 设计的图形化界面。

> **注意**：项目尚在开发中，部分功能仍在完善。

---

## 功能特性

- **Python 脚本集成**  
  通过 `PyExecutor` / `PyThreadPoolExecutor` 调用 Python 模块中的类和方法，支持同步、异步、线程池执行。

- **配置系统**  
  采用 `MultiConfigManager` 管理多个 JSON 配置文件，支持优先级覆盖、热重载、配置变更监听。

- **日志与调试**  
  `DebugLog` 提供模块化日志等级控制，可输出到控制台；通过 `Console` 类可在 Windows 上创建调试控制台。

- **WebSocket 通信**  
  附带的 `WebSocketCore.py` 演示了与 DGLab 服务的基本 WebSocket 交互（连接、心跳、绑定、控制命令）。

- **跨平台构建**  
  基于 CMake，支持 Windows、Linux 等平台（需自行适配 Qt 和 Python 路径与附加包文件等内容）。

---

## 依赖项

### 系统依赖
- **C++ 编译器**：支持 C++20 标准（如 GCC 10+、Clang 12+、MSVC 2022）
- **Qt**：5.15 或 6.x（Core、Gui、Widgets）
- **Python**：3.9 或更高版本（开发库及解释器）

### 第三方库
- **[pybind11](https://github.com/pybind11/pybind11)** (版本 3.0.1)  
  用于 C++ 与 Python 交互。**注意**：该库未包含在仓库中，需手动下载或作为 Git 子模块添加。  
  下载地址：[https://github.com/pybind11/pybind11/archive/refs/tags/v3.0.1.tar.gz](https://github.com/pybind11/pybind11/archive/refs/tags/v3.0.1.tar.gz)  
  解压后重命名为 `pybind11-3.0.1` 并放置于项目根目录。

- **[nlohmann/json](https://github.com/nlohmann/json)** (版本 3.11.2)  
  用于 JSON 解析。**注意**：项目未包含单头文件 `include/json.hpp`，需额外下载。  
  下载地址：[https://github.com/nlohmann/json](https://github.com/nlohmann/json)  
  解压后重命名为 `json.hpp` 并放置于项目 `include/` 目录。

### Python 包依赖
示例脚本 `python/WebSocketCore.py` 使用了以下 Python 库，请确保已安装：

- `websockets` (建议版本 10.0 或更高)  
  安装命令：`pip install websockets`

其他 Python 标准库（如 `asyncio`, `json`, `logging`）无需额外安装。

---

## 快速开始

### 1. 获取源码

```bash
git clone https://github.com/yourusername/DG-LAB-Client.git
cd DG-LAB-Client
```

### 2. 准备第三方库

- 下载 [pybind11 3.0.1](https://github.com/pybind11/pybind11/archive/refs/tags/v3.0.1.tar.gz) 并解压到项目根目录，确保文件夹名为 `pybind11-3.0.1`。
- 下载 [nlohmann/json](https://github.com/nlohmann/json)并解压到项目 `include/` 目录，确保文件名为 `json.hpp`。

### 3. 安装 Python 依赖

```bash
pip install websockets
```

### 4. 配置 CMake

项目使用 CMake 构建。建议使用 CMake GUI 或命令行：

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt -DPython_ROOT=/path/to/python
```

- 根据需要指定 Qt 和 Python 的路径。
- 如果使用 Qt6，CMake 会自动查找；若使用 Qt5，确保安装了 `qt5-default` 等包。

### 5. 编译

```bash
cmake --build . --config Release
```

编译完成后，可执行文件将位于 `build/bin/` 或 `build/Release/` 下，同时 `python/` 和 `config/` 目录会被复制到输出目录。

### 6. 运行

进入输出目录，执行：

```bash
./DG-LAB-Client
```

首次运行会自动生成默认配置文件 `config/main.json`，您可以根据需要修改。

---

## 使用说明

### 配置文件

系统包含三个配置文件，按优先级从低到高依次为：

- `main.json`：主配置，优先级 0
- `system.json`：系统配置，优先级 1
- `user.json`：用户配置，优先级 2

高优先级的配置项会覆盖低优先级的同路径配置。配置文件中 `__priority` 字段用于定义优先级，不可删除。

示例 `main.json`：

```json
{
    "__priority": 0,
    "app": {
        "name": "DG-LAB-Client",
        "version": "1.0.0",
        "debug": true,
        "log": {
            "level": 0,
            "only_type_info": false
        }
    },
    "python": {
        "path": "python"
    }
}
```

### Python 脚本

默认 Python 脚本存放于可执行文件所在目录的 `python/` 文件夹中。  
示例脚本 `WebSocketCore.py` 提供了 `DGLabClient` 类，可通过 C++ 调用其方法进行 WebSocket 连接与控制。

在 `main.cpp` 中，您可以通过 `PyExecutorManager` 注册并调用 Python 方法：

```cpp
PyExecutorManager manager;
manager.register_executor("WebSocketCore", "DGLabClient", false);
manager.call_sync<void>("WebSocketCore", "DGLabClient", "set_ws_url", "ws://localhost:9999");
bool connected = manager.call_sync<bool>("WebSocketCore", "DGLabClient", "connect");
```

### 日志

日志等级通过配置文件 `app.log.level` 控制：

- 0：DEBUG
- 1：INFO
- 2：WARN
- 3：ERROR
- 4：NONE

您可以在代码中使用 `LOG_MODULE` 宏输出日志：

```cpp
LOG_MODULE("MyModule", "my_function", LOG_INFO, "This is a log message.");
```

### 调试控制台

在 Windows 上，如果配置文件中的 `app.debug` 为 `true`，程序启动时会自动创建一个调试控制台，用于显示日志输出。

---

## 本地 WebSocket 中转服务部署

本项目作为客户端，需要连接一个本地的 WebSocket 中转服务才能与 DGLab App 进行通信。该中转服务基于 Node.js 开发，您需要先在电脑上启动它。

### 1. 服务说明

此服务（默认文件名为 `websocketNode.js`）扮演着中央中转站的角色：
1.  它接收本客户端（DG-LAB-Client）的连接。
2.  它接收 DG-Lab 官方手机 App 的连接（官方似乎只有 3.x 支持 websocket）。
3.  它负责在两者之间中继消息（如绑定指令、强度控制、心跳包等），使电脑和手机能通过 WebSocket 进行数据交换。

### 2. 环境准备

-   **安装 Node.js**：确保您的系统已安装 [Node.js](https://nodejs.org/) (建议版本 12.x 或更高)。
-   **获取服务脚本**：从 [DG-LAB-OPENSOURCE 仓库](https://github.com/DG-LAB-OPENSOURCE/DG-LAB-OPENSOURCE/tree/main/socket/BackEnd(Node)) 下载 `websocketNode.js` 文件，并将其保存到您电脑上的一个专用文件夹中（例如 `DGLab-server/`）。

### 3. 安装与启动

打开终端（命令提示符或 PowerShell），进入存放 `websocketNode.js` 的文件夹，执行以下步骤：

**步骤一：安装依赖**
该服务通常依赖 `ws` 库（WebSocket 实现）。运行以下命令安装依赖：
```bash
npm install ws
```
如果项目中还包含 `package.json` 文件，可以直接运行 `npm install` 安装所有依赖。

**步骤二：启动服务**
在同一个目录下，运行以下命令启动服务：
```bash
node websocketNode.js
```
**端口设置**：服务启动后，默认会在控制台输出其监听的端口。**请仔细查看控制台日志**，默认端口通常是 `9999`，具体请以您下载的脚本中定义的端口为准。

**成功标志**：如果看到类似 `新的 WebSocket 连接已建立，标识符为:xxx` 的提示，则说明服务已成功运行。

### 4. 配置与连接本客户端

1.  保持 Node.js 服务在后台运行（终端窗口不要关闭）。
2.  启动本客户端（DG-LAB-Client）。
3.  客户端会根据配置文件（`config/main.json` 中的 `python.path`）或设置加载 Python 脚本。
4.  Python 脚本（如 `WebSocketCore.py`）中需要配置正确的 WebSocket 服务地址。
5.  客户端连接成功后，会生成包含服务地址和 `clientId` 的二维码内容。

### 5. 手机 App 连接

1.  确保您的手机与电脑连接在**同一个局域网**下。
2.  打开 DGLab 官方手机 App（官方似乎只有 3.x 支持 websocket）。
3.  使用 App 内的扫一扫功能，扫描本客户端生成的二维码。
4.  扫描后，手机 App 将自动连接到您电脑上运行的中转服务，并与本客户端建立绑定关系。后续的强度调节、波形发送等操作即可进行。

---

## 项目结构

```
DG-LAB-Client/
├── CMakeLists.txt          # 主构建文件
├── include/                 # 公共头文件
│   ├── AppConfig.h
│   ├── ConfigManager.h
│   ├── PyExecutor.h
│   ├── ...
├── src/                     # 源文件
│   ├── AppConfig.cpp
│   ├── ConfigManager.cpp
│   ├── PyExecutor.cpp
│   ├── ...
├── python/                  # Python 脚本
│   └── WebSocketCore.py
├── config/                  # 默认配置文件
│   ├── main.json
│   ├── system.json
│   └── user.json
├── pybind11-3.0.1/          # pybind11 源码（需手动放置）
├── LICENSE.txt
└── README.md
```

---

## 截图

（暂时留空）

---

## 贡献指南

欢迎提交 Issue 和 Pull Request。在贡献前请确保：

- 代码遵循现有风格（缩进 4 空格，使用 `#pragma once`）。
- 添加或修改功能时更新相关文档。
- 确保本地测试通过。

---

## 许可证

本项目基于 MIT 许可证开源，详情请参阅 [LICENSE.txt](LICENSE.txt) 文件。

---

## 联系方式

- 作者：[CrimsonSeraph]
- BiliBili：[浪天幽影(UID：1741002917)](https://space.bilibili.com/1741002917?spm_id_from=333.1007.0.0)
- X：[𝒞𝓇𝒾𝓂𝓈𝑜𝓃𝒮𝑒𝓇𝒶𝓅𝒽✟升天✟(@CrimSeraph_QwQ)](https://x.com/CrimSeraph_QwQ)
- 项目主页：[https://github.com/CrimsonSeraph/DG-LAB-Client](https://github.com/CrimsonSeraph/DG-LAB-Client)
