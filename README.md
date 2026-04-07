# DG-LAB-Client

> **目录**  
> - [一、项目简介](#一项目简介)  
> - [二、功能特性](#二功能特性)  
> - [三、依赖项](#三依赖项)  
> - [四、快速开始](#四快速开始)  
> - [五、自动化构建](#五自动化构建)  
> - [六、使用说明](#六使用说明)  
> - [七、本地 WebSocket 中转服务部署说明](#七本地-websocket-中转服务部署)
> - [八、项目结构](#八项目结构)  
> - [九、截图](#九截图)  
> - [十、编码规范](#十编码规范)  
> - [十一、FAQ](#十一faq)  
> - [十二、贡献指南](#十二贡献指南)  
> - [十三、许可证](#十三许可证)  
> - [十四、联系方式](#十四联系方式)

一个基于 Qt 的桌面客户端，用于与 DG-Lab 服务进行 WebSocket 通信。  
项目采用 C++20 编写，通过启动独立的 Python 子进程（`Bridge.py`）来管理与 DG-Lab 服务器的 WebSocket 连接。  
实现了多级配置管理、模块化日志、异步任务以及灵活的规则引擎等功能。  
当前版本号：`v0.2.0`  
> 详细请查看： [更新日志](ChangeLog.md)。

---

## 一、项目简介

DG-LAB-Client 是一个为 DG-Lab（地牢实验室）设计的客户端工具，它通过 **Python 子进程 + TCP 通信** 的方式与 DG-Lab 的 WebSocket 服务交互，支持：

- 启动独立的 Python 进程（`Bridge.py`），该进程作为 TCP 服务器监听本地端口，接收 C++ 主程序发送的命令，并调用 WebSocket 客户端与远程服务通信。
- 多配置文件管理（主配置、系统配置、用户配置），支持优先级覆盖。
- 调试控制台与分级日志（可输出到控制台或 Qt 界面）。
- 通过 Qt 设计的图形化界面，按钮操作通过信号槽触发异步 Python 调用，避免 UI 阻塞。
- **规则引擎**：支持从 JSON 文件中加载带占位符 `{}` 的模式规则，动态填充参数生成指令字符串，便于灵活配置波形或强度操作。

> **注意**：当前主版本号为 0（表示开发测试阶段，尚不具备获取数据功能，只有处理数据、发送指令能力）。

---

## 二、功能特性

- **Python 子进程通信**  
  通过 `PythonSubprocessManager` 启动外部 Python 脚本（`Bridge.py`），脚本启动后输出监听端口，主程序通过 `QTcpSocket` 连接，以 JSON 格式发送命令并接收响应。所有耗时调用均放入全局线程池执行，完成后通过信号槽返回主线程。

- **配置系统**  
  采用 `MultiConfigManager` 管理多个 JSON 配置文件（main/user/system），支持优先级覆盖、热重载、配置变更监听。配置项通过 `ConfigValue<T>` 或 `ConfigObject<T>` 包装，提供类型安全访问和缓存。  
  规则表格高级编辑：在“配置”页面的规则表格中，“通道”和“模式”列使用下拉框选择，“值模式”列使用可视化公式构建器。支持通过按钮快速插入 {}、+-*/()，并在保存时自动检查括号平衡合法性，极大提升了复杂计算式（如 {}+{}*2、({}*2)+{}）的编辑体验。

- **规则引擎**  
  提供 `Rule` 类（支持 `{}` 占位符）和单例 `RuleManager`。可从指定目录下扫描 JSON 规则文件（含关键字 `rule`），加载规则集，并支持创建/删除/切换规则文件。规则可用于动态生成发送给 Python 子进程的命令（如强度操作、波形参数），极大提升了操作的灵活性。

- **日志与调试**  
  `DebugLog` 提供模块化日志等级控制，可输出到控制台、Qt 界面等不同的 `LogSink`；通过 `Console` 类可在 Windows 上创建调试控制台。

- **WebSocket 通信**  
  Python 脚本 `Bridge.py` 内部使用 `WebSocketCore.py`（工具库）与 DG-Lab 服务进行 WebSocket 交互（连接、心跳、绑定、控制命令），并将结果通过 TCP 返回给 C++ 主程序。

- **跨平台构建**  
  基于 CMake，支持 Windows、Linux、macOS 等平台，并通过 GitHub Actions 自动构建和打包。

---

## 三、依赖项

### 1. 系统依赖
- **C++ 编译器**：支持 C++20 标准（如 GCC 10+、Clang 12+、MSVC 2022）
- **Qt**：5.15 或 6.x（Core、Gui、Widgets、Network）
- **Python**：3.9 或更高版本

**注意**：Python 需要安装在系统环境中，并且 `python` 命令可用。CMake 配置时会自动查找 Python 解释器路径。

> 推荐使用 VS2022/2026（MSVC）进行 Windows 开发，Linux/macOS 可使用 GCC 或 Clang。  
> 👉 遇到依赖问题？请查看 [常见问题 - 编译与运行](#编译与运行)

### 2. 第三方库
- **[nlohmann/json](https://github.com/nlohmann/json)** (版本 3.12.0)  
  用于 JSON 解析。CMake 会在配置时自动从 GitHub 下载单头文件到构建目录 `/include/nlohmann/` 

### 3. Python 包依赖
Python 子进程（`Bridge.py`）需要以下库，由 CI 自动安装或手动部署：
- `websockets` (建议版本 10.0 或更高)  
- `qrcode[pil]` (用于生成二维码)  
  安装命令：

```bash
pip install websockets qrcode[pil]
```

---

## 四、快速开始

### 1. 获取源码

```bash
git clone https://github.com/yourusername/DG-LAB-Client.git
cd DG-LAB-Client
```

### 2. 安装 Python 依赖

```bash
pip install websockets qrcode[pil]
```

### 3. 配置 CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt -DPython_ROOT_DIR=/path/to/python
```

- 对于 Qt6，CMake 通常能自动找到；若使用 Qt5，请确保 `Qt5` 包可用。
- 可通过 `-DPYTHON_PACKAGES_DIR=path/to/site-packages` 指定要打包的第三方 Python 包目录（可选，供 CI 使用）。

> 👉 配置或编译失败？请查看 [常见问题 - 编译与运行](#编译与运行)

### 4. 编译

```bash
cmake --build . --config Release
```

### 5. 运行

编译完成后，可执行文件位于 `build/Release`（Windows）或 `build`（Linux/macOS）目录下。运行时需要确保以下目录与可执行文件同级：

- `config/`：包含 `main.json`、`system.json`、`user.json` 以及规则子目录（如 `config/rules/`）
- `python/`：包含 `Bridge.py` 和 `WebSocketCore.py`
- `qcss/`：包含 `style.qcss`
- `assets/`：包含资源文件（如图片）

CMake 的 `POST_BUILD` 命令会自动复制这些目录到输出目录。

### 6. 打包（生成安装包）

```bash
cpack
```

生成的安装包将位于 `build/` 目录下（根据平台不同，可能是 .exe、.dmg、.deb 等）。

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

> 若配置文件丢失将从默认配置（`DefaultConfigs.cpp`）加载，并在程序运行时自动生成缺失的配置文件。  
> 👉 配置文件相关问题请查看 [常见问题 - 配置问题](#配置问题)

<details>
<summary> 示例 `main.json` </summary>

```json
{
    "__priority": 0,
    "app": {
        "name": "DG-LAB-Client",
        "version": "1.0.0",
        "debug": false,
        "log": {
            "console_level": 0,
            "only_type_info": false,
            "ui_log_level": 0
        }
    },
    "python": {
        "path": "python",
        "bridge_path": "./python/Bridge.py"
    },
    "rule": {
        "path": "./config/rules",
        "key": "rule"
    },
    "version": "1.0",
    "DGLABClient": "DG-LAB-Client"
}
```

</details>

**注意：** 其中 `"version"` 与 `"DGLABClient"` 为检查字段，内容随意，但 **请勿删除或修改** 此字段。

### 2. Python 脚本

- `python/Bridge.py`：主入口脚本，启动 TCP 服务器，等待 C++ 客户端连接，解析命令并调用 `WebSocketCore.py` 中的 `DGLabClient` 类。
- `python/WebSocketCore.py`：WebSocket 客户端核心库，封装了与 DG-Lab 服务器的连接、心跳、绑定、强度控制等逻辑。

### 3. 规则引擎

规则引擎允许您通过 JSON 文件定义一系列带占位符 `{}` 的规则，在运行时传入参数动态生成字符串（例如构造 `send_strength` 命令的 JSON）。使用步骤如下：

- **规则文件存放**：默认规则目录为 `config/rules/`（可通过 `rule.path` 配置修改）。目录下的 JSON 文件若文件名包含 `rule` 关键字（可通过 `rule.key` 配置），则会显示在 UI 的规则文件列表中。
- **规则格式**：每个规则文件应包含一个 `"rules"` 对象，键为规则名称，值为模式字符串。

<details>
<summary> 示例 `rules.json` </summary>

  ```json
  {
    "rules": {
      "wave_short": "{\"cmd\":\"send_pulse\",\"channel\":1,\"pulses\":[\"01020304\"],\"duration\":{}}",
      "strength_up": "{\"cmd\":\"send_strength\",\"channel\":{},\"mode\":1}"
    }
  }
  ```

</details>

- **规则占位符**：模式中的 `{}` 会被按顺序替换为参数。例如 `strength_up` 规则需要一个参数（通道号），调用 `RuleManager::evaluate("strength_up", 2)` 将生成 `{"cmd":"send_strength","channel":2,"mode":1}`。
- **UI 操作**：在客户端的“配置”页面中，您可以：
  - 从下拉列表切换规则文件。
  - 新建/删除/保存规则文件。
  - 添加/编辑规则：规则名称使用普通文本输入，“通道”和“模式”使用下拉选择，“值模式”双击后弹出公式构建对话框（支持按钮快速插入符号 + 实时括号合法性检查）。

- **在代码中使用**：
  ```cpp
  #include "RuleManager.h"
  // 初始化（AppConfig 初始化后调用）
  RuleManager::instance().init();
  // 加载规则文件（默认为 rules.json）
  RuleManager::instance().load_rule_file("rules.json");
  // 评估规则
  std::string result = RuleManager::instance().evaluate("strength_up", 1);
  // 将 result 作为命令发送给 Python 子进程
  ```

> **注意**：规则文件可能包含任意命令，不要加载不可信的 JSON 文件！  
> 👉 规则引擎相关问题请查看 [常见问题 - 规则引擎问题](#规则引擎问题)

### 4. 日志

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

> 👉 日志不显示或等级不生效？请查看 [常见问题 - 通用问题](#通用问题)

### 5. 调试控制台

在 Windows 上，如果配置文件中的 `app.debug` 为 `true`，程序启动时会自动创建一个调试控制台，用于显示详细的日志输出。  
**注意：** 该控制台使用 `#include <windows.h>` 仅在 Windows 平台上可用，并且需要在配置文件中启用调试模式。  
当然，应用中首页也会输出日志到 Qt 界面，您可以根据需要选择查看。

---

## 七、本地 WebSocket 中转服务部署

### 1. 服务说明

此 WebSocket 服务扮演中央中转站的角色，负责在电脑客户端（第三方终端）与 DG-Lab 手机 App 之间中继消息（如绑定指令、强度控制、心跳包等），使两者能通过 WebSocket 进行数据交换。该服务仅支持 **郊狼脉冲主机 3.0**。

### 2. 环境准备

- **安装 Node.js**：建议版本 12.x 或更高。您可以在 [Node.js 官网](https://nodejs.org/) 下载并安装。
- **获取服务脚本**：从官方仓库获取后端代码，项目结构如下：

<details>
<summary> 官方仓库后端代码项目结构 </summary>

  ```
  socket/
  └── v2/                          # ✅ 推荐使用
      ├── backend/                 # WebSocket 后端 (Node.js)
      │   ├── src/
      │   │   ├── index.js         # 主入口，启动服务器 & 消息路由
      │   │   ├── config.js        # 配置管理（支持 .env 环境变量）
      │   │   ├── connection.js    # 连接管理（注册、配对、断开）
      │   │   ├── message.js       # 消息处理（验证、转发、强度/波形）
      │   │   ├── timer.js         # 定时器管理（波形消息队列发送）
      │   │   └── logger.js        # 日志模块（winston）
      │   └── package.json
      └── frontend/                # 前端控制页面 (HTML+CSS+JS)
  ```

</details>

### 3. 安装与启动

打开终端，进入 `socket/v2/backend` 目录，执行以下步骤：

**步骤一：安装依赖**

```bash
cd socket/v2/backend
npm install
```

**步骤二：配置环境变量（可选）**

在 `socket/v2/backend` 目录下创建 `.env` 文件，可配置以下参数：

| 变量 | 默认值 | 说明 |
| - | - | - |
| `PORT` | 9999 | WebSocket 服务端口 |
| `HEARTBEAT_INTERVAL` | 60000 | 心跳间隔（毫秒） |
| `DEFAULT_PUNISHMENT_TIME` | 1 | 波形发送频率（每秒次数） |
| `DEFAULT_PUNISHMENT_DURATION` | 5 | 波形默认持续时间（秒） |
| `LOG_LEVEL` | info | 日志级别 |

**步骤三：启动服务**

```bash
# 生产模式
npm start

# 开发模式（自动重启）
npm run dev
```

服务默认监听端口 `9999`。

### 4. 客户端配置与连接

1. 保持 Node.js 服务在后台运行（终端窗口不要关闭）。
2. 启动您的客户端程序（如 DG-LAB-Client 或其他第三方终端）。
3. 客户端连接 WebSocket 服务后，服务端会分配 `clientId` 并返回给客户端。
4. 客户端根据 WebSocket 地址和 `clientId` 生成二维码，格式如下：
   ```
   https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://你的服务器地址:端口/clientId
   ```

### 5. 手机 App 连接

1. 确保手机与电脑连接在 **同一个局域网** 下。
2. 打开 DG-Lab 官方手机 App（需要 3.x 及以上版本，支持 WebSocket）。
3. 在 App 中打开 SOCKET 功能 → 点击连接服务器 → 扫描客户端生成的二维码。
4. 扫描后，App 将自动连接到中转服务并完成配对。

配对成功后，您即可进行强度调节、波形发送等操作。

### 6. 注意事项

1. **客户端 ID 必须唯一**：服务端生成的 `clientId` 必须全局唯一，推荐使用 UUID v4。

2. **消息格式要求**：除初始连接时 `targetId` 可为空外，所有消息必须包含 `type`、`clientId`、`targetId`、`message` 四个字段且值不为空。

3. **本地调试与正式使用**：
   - 本地调试时可以使用 `ws://` 协议
   - 若需公网访问，建议使用 `wss://` 协议以保证通信安全

4. **保持服务运行**：启动服务的终端窗口不要关闭，关闭后服务会终止。

5. **检查防火墙**：如果手机无法连接，检查电脑防火墙是否允许 WebSocket 端口（默认 9999）的入站连接。

6. **默认端口确认**：服务默认监听 `9999` 端口，具体请以您下载的脚本中实际定义的端口为准。

7. **规则文件安全**：不要删除默认的 `rules.json`，删除其他规则文件前请确保已保存重要规则。

**相关资源**：
- 官方仓库：[https://github.com/DG-LAB-OPENSOURCE/DG-LAB-OPENSOURCE](https://github.com/DG-LAB-OPENSOURCE/DG-LAB-OPENSOURCE)
- 常见问题文档：`socket/QA/Websocket_open_source_QA_Chinese.txt`（位于仓库中）

如有其他问题，请联系官方邮箱：service@dungeon-lab.com

> 👉 部署或连接过程中遇到问题？请查看 [常见问题 - 本地 WebSocket 中转服务](#本地-websocket-中转服务)

---

## 八、项目结构

<details>
<summary> 目录树 </summary>

```
DG-LAB-Client/
├── CMakeLists.txt              # 主构建文件
├── qt.cmake                    # Qt 相关设置
├── include/                    # 公共头文件
│   ├── AppConfig.h
│   ├── AppConfig_impl.hpp
│   ├── ComboBoxDelegate.h
│   ├── ConfigManager.h
│   ├── ConfigStructs.h
│   ├── Console.h
│   ├── DebugLog.h
│   ├── DebugLog_utils.hpp
│   ├── DefaultConfigs.h
│   ├── DGLABClient.h
│   ├── DGLABClient_utils.hpp
│   ├── FormulaBuilderDialog.h
│   ├── MultiConfigManager.h
│   ├── MultiConfigManager_impl.hpp
│   ├── PythonSubprocessManager.h
│   ├── Rule.h
│   ├── RuleManager.h
│   ├── RuleManager_impl.hpp
│   └── ValueModeDelegate.h
├── src/                        # 源文件
│   ├── AppConfig.cpp
│   ├── ComboBoxDelegate.cpp
│   ├── ConfigManager.cpp
│   ├── ConfigStructs.cpp
│   ├── Console.cpp
│   ├── DebugLog.cpp
│   ├── DefaultConfigs.cpp
│   ├── DGLABClient.cpp
│   ├── FormulaBuilderDialog.cpp
│   ├── MultiConfigManager.cpp
│   ├── PythonSubprocessManager.cpp
│   ├── Rule.cpp
│   ├── RuleManager.cpp
│   └── ValueModeDelegate.cpp
├── python/                     # Python 脚本
│   ├── Bridge.py
│   └── WebSocketCore.py
├── config/                     # 默认配置文件
│   ├── main.json
│   ├── system.json
│   ├── user.json
│   └── rules/                  # 规则文件目录（自动创建）
│       └── rules.json
├── assets/                     # 资源文件
│   └── normal_image/
│       └── main_image.png
├── qcss/                       # Qt 样式表
│   └── style_light.qcss
│   └── style_night.qcss
├── DGLABClient.qrc              # Qt 资源文件
├── LICENSE.txt                  # 许可证文件
├── README.md                    # 项目说明文档
├── .editorconfig                # 编辑器配置
├── .gitattributes               # Git 属性配置
├── .gitignore                   # Git 忽略规则
└── .github/workflows/build.yml  # GitHub Actions 工作流
```

</details>

---

## 九、截图

（暂时留空）

---

## 十、编码规范

- **文件编码与换行**：UTF-8 without BOM，换行符使用 CRLF（Windows 风格），文件末尾保留一个空行。
- **缩进与空白**：使用 4 个空格缩进（不使用 Tab），左括号采用 K&R 风格（与语句同行）。
- **命名规则**：类名用 `PascalCase`，变量/函数名用 `snake_case`，常量/宏用 `UPPER_CASE`；Python 模块名使用 `PascalCase`。
- **导入顺序**：自定义模块 > 第三方库 > 标准库，每组内按字母顺序排列。
- **注释规范**：独占一行的注释 `//` 后跟空格，与代码同缩进；行尾注释与代码间隔一个 Tab，`//` 后跟空格。

详细的编码规范请参阅项目根目录下的 **[CodingStyle.md](CodingStyle.md)**。

---

## 十一、FAQ

### 编译与运行

<details>
<summary><b>Q1: CMake 配置时找不到 Qt</b></summary>

**现象**：
```
Could not find a package configuration file provided by "Qt6" or "Qt5"
```

**解决方案**：
- 确保 Qt 已正确安装，并且 `CMAKE_PREFIX_PATH` 指向 Qt 的安装目录（例如 `C:/Qt/6.5.0/msvc2019_64`）。
- 或者设置环境变量 `Qt6_DIR` / `Qt5_DIR`。
- 在 Linux 上可以通过包管理器安装 Qt 开发包（如 `qt6-base-dev`），CMake 通常能自动找到。

</details>

<details>
<summary><b>Q2: Python 子进程启动失败，提示找不到模块</b></summary>

**现象**：
程序启动后日志显示 `Bridge.py` 报错 `ModuleNotFoundError: No module named 'websockets'` 或 `qrcode`。

**解决方案**：
- 确认已执行 `pip install websockets qrcode[pil]`。
- 检查 Python 环境：确保运行客户端时使用的 Python 解释器与安装依赖的是同一个。可以在命令行执行 `pip show websockets` 查看安装位置。
- 若使用虚拟环境，需要在 CMake 配置时指定 `-DPython_ROOT_DIR` 指向虚拟环境的路径。

</details>

<details>
<summary><b>Q3: nlohmann/json 下载失败</b></summary>

**现象**：
CMake 配置过程中报错，无法从 GitHub 下载 `json.hpp`。

**解决方案**：
- 检查网络连接，确保能够访问 `raw.githubusercontent.com`。
- 手动下载 [json.hpp](https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp) 并放入构建目录的 `/include/nlohmann/` 下（根据 CMake 输出路径调整）。
- 或者修改 `CMakeLists.txt`，改用本地已下载的头文件路径。

</details>

<details>
<summary><b>Q4: 编译时出现 C++20 相关语法错误</b></summary>

**现象**：
编译器报错如 `'std::span' is not a member of 'std'` 或要求 C++20 标准。

**解决方案**：
- 确认编译器版本：GCC ≥10、Clang ≥12、MSVC ≥2022。
- 在 CMake 配置时显式指定 C++ 标准：`-DCMAKE_CXX_STANDARD=20`。
- 若使用旧版 IDE（如 VS2019），需要升级到 VS2022 或安装支持 C++20 的工具集。

</details>

### 配置问题

<details>
<summary><b>Q5: 配置文件修改后不生效</b></summary>

**现象**：
修改了 `user.json` 中的某个值，但程序运行时使用的还是旧值。

**解决方案**：
- 确认修改的文件是正确的（注意优先级：`user.json` > `system.json` > `main.json`）。如果 `system.json` 或 `main.json` 中定义了相同路径的配置，它们会被覆盖，但不会删除。
- 重新执行 CMake 构建项目或手动将修改后的配置文件复制到构建目录的 `/config/` 下。
- 检查 JSON 格式是否正确（如多余的逗号），可以使用在线 JSON 校验工具。

</details>

<details>
<summary><b>Q6: 配置文件丢失后如何恢复？</b></summary>

**现象**：
误删了 `config/` 目录下的某个 JSON 文件，程序启动报错或使用默认值。

**解决方案**：
- 程序内置了默认配置（定义在 `DefaultConfigs.cpp`），丢失的文件会在运行时自动重新生成（前提是目录存在）。只需确保 `config/` 目录可写。
- 也可以从源码仓库中复制 `config/` 目录下的示例文件到运行目录。

</details>

<details>
<summary><b>Q7: 更改客户端所使用的端口遇到问题？</b></summary>

**更改方法**：

- 编译运行客户端后，打开 `配置` 页面，在端口中输入 **你需要更换的端口** 并点击右侧的 `确定`，若显示端口更改成功即完成更改。
- 在默认的配置文件 `system.json` 中 `app.websocket.port` 即是所用端口

> 若在项目根目录更改，请确保构建输出目录下的配置文件也应用更改！
> 端口范围建议使用 1024~65535 之间的数字，避免使用系统保留端口（0~1023）。

**现象**

通过上述方式设置了端口，但应用的仍然是旧的或默认端口 `9999`。

**解决方法**

- 点击 `确定` 后根据 `错误信息` 更换端口再次点击 `确定` 尝试更改。
- 尝试重新执行 CMake 构建项目或手动自定义的 JSON 规则文件放入构建目录的 `config/` 下。

</details>

### 规则引擎问题

<details>
<summary><b>Q8: 规则文件在 UI 中不显示</b></summary>

**现象**：
将自定义的 JSON 规则文件放入 `config/rules/` 目录，但在客户端的规则页面中看不到。

**解决方案**：
- 检查文件名是否包含默认配置文件 `main.json` 中项 `rule.key` 指定的关键字（默认为 `rule`）。例如 `my_rules.json` 包含 `rule` 字样，可以被识别；`settings.json` 则不会。
- 确认规则文件的 JSON 格式正确，且根对象包含 `"rules", "version", "DGLABClient"` 键。
- 尝试重新执行 CMake 构建项目或手动自定义的 JSON 规则文件放入构建目录的 `config/rules/` 下。

</details>

<details>
<summary><b>Q9: 规则评估生成的命令不符合预期</b></summary>

**现象**：
调用 `RuleManager::evaluate_command("rule_name", args...)` 返回的 `QJsonObject` 中 `value` 字段的值不正确（如始终为 0 或未按公式计算），或者通道/模式与设置不符。

**可能原因及解决方案**：

| 原因 | 解决方法 |
| - | - |
| 传入的参数个数与规则中的 `{}` 占位符数量不匹配 | 检查规则模式中 `{}` 的个数，确保传入参数数量一致。若参数不足，表达式中的剩余 `{}` 不会被替换，导致求值失败（日志会输出“参数数量不匹配”错误）。 |
| 值表达式求值失败（如语法错误、除零等） | 查看日志中的 `Rule::computeValue` 错误信息，表达式错误会输出 `表达式求值失败: ...`。修正 `value_pattern` 中的表达式（例如确保运算符正确、括号匹配）。 |
| 计算结果被模式钳位规则修正 | 模式 2（“设为”）将值钳位到 [0, 200]；模式 3/4（“连减”/“连增”）钳位到 [1, 100]。如果期望的值超出范围，会被自动限制，这是正常行为。 |
| 通道无效（非 "A" 或 "B"） | `Rule::normalize_channel` 会将通道规范化为 "A"/"B"，无效通道（如 "C"、空字符串）会导致命令中使用默认通道 1（即 A）。检查规则定义中的 `channel` 字段是否为 "A" 或 "B"（大小写不敏感）。 |
| 模式值超出 0~4 范围 | 模式仅支持 0~4，若配置了其他值，行为未定义。请在规则文件中使用正确的模式编号。 |

**调试建议**：
- 启用 DEBUG 日志级别（`app.log.console_level = 0`），查看 `RuleManager` 和 `Rule` 模块输出的详细日志，包括加载的规则参数、表达式求值过程和结果。
- 使用 `RuleManager::get_rule_value_pattern("rule_name")` 获取原始表达式，手动验证计算逻辑。
- 若使用 Qt 界面中的公式构建器，注意保存前检查括号平衡性。

</details>

<details>
<summary><b>Q10: 公式构建器中的合法性检查报错</b></summary>

**现象**：
在“配置”页面的“值模式”列双击打开公式构建器，无法保存。

**可能原因及解决方案**：

| 原因 | 解决方法 |
| - | - |
| 表达式中包含空格（如 `a + b`） | 移除所有空格，或使用花括号包裹变量名（如 `{a}+{b}`） |
| 变量名未用花括号包裹（如直接写 `a`、`price`） | 所有变量必须写成 `{变量名}` 形式，例如 `{a}+{b}` |
| 使用了非法字符（如字母、数字、小数点、等号等未在 `+ - * / ( ) { }` 中的符号） | 只允许运算符、括号和花括号；变量必须放入 `{}` 内，常量数字需确保无非法字符（代码不支持直接写数字，但可通过 `{数字}` 变通） |
| 表达式为空 | 输入有效的公式表达式 |
| 括号不匹配（缺少右括号或有多余右括号） | 检查左右括号数量是否一致，确保 `(` 后有对应的 `)` |
| 运算符连续出现（如 `a++b`）或出现在开头/结尾（如 `+a` 或 `a+`） | 检查运算符是否成对出现在操作数之间，不能以运算符开头或结尾 |
| 缺少闭合的 `}`（如 `{a`） | 确保每个 `{` 都有对应的 `}` |
| 左括号位置不正确（如 `a(` 或 `{a}(` 等连续操作数后紧跟左括号） | 左括号前必须是运算符或表达式开头，不能直接跟在操作数后 |
| 右括号前缺少操作数（如 `()` 或 `a+()`） | 括号内必须有有效的表达式，不能为空 |
| 表达式以运算符结尾（如 `a+`） | 最后一个字符不能是运算符 |

</details>

### Python 子进程通信问题

<details>
<summary><b>Q11: 主程序无法连接到 Python 子进程（TCP 连接失败）</b></summary>

**现象**：
日志显示 `Failed to connect to Python bridge: Connection refused`。

**解决方案**：
- 确认 `Bridge.py` 是否正常启动。查看控制台输出（如果开启了调试控制台）。
- 检查端口是否被占用。`Bridge.py` 默认使用第一个可用的端口（从 5000 开始递增），主程序会从子进程的 stdout 解析端口号。
- 防火墙可能阻止了本地回环连接，尝试暂时关闭防火墙测试。

</details>

<details>
<summary><b>Q12: Python 子进程启动后立即退出</b></summary>

**现象**：
主程序启动后日志显示 `Python process exited with code X`。

**解决方案**：
- 手动运行 `python Bridge.py` 查看错误输出。常见原因：缺少依赖（`websockets`）、Python 版本过低（需要 3.9+）、文件路径错误。
- 确保 `WebSocketCore.py` 与 `Bridge.py` 在同一目录下。
- 检查配置文件中的 `python.bridge_path` 是否正确指向 `Bridge.py`。

> 如："bridge_path": "./python/Bridge.py"

</details>

### 本地 WebSocket 中转服务

<details>
<summary><b>Q13: 端口被占用，无法启动服务</b></summary>

**现象**：
启动 Node.js 服务时报错 `Error: listen EADDRINUSE: address already in use :::9999`。

**解决方案**：
- 检查是否有其他程序占用了 9999 端口，可以通过 `lsof -i :9999`（Linux/macOS）或 `netstat -ano | findstr :9999`（Windows）命令查看。
- 更换其他端口，在 `.env` 文件中修改 `PORT` 配置。

> 若更换端口，请修改客户端所使用的端口，详细请参考 [常见问题 - 配置问题](#配置问题)

</details>

<details>
<summary><b>Q14: 手机 App 无法连接到 WebSocket 服务</b></summary>

**可能原因及解决**：

| 原因 | 解决方法 |
| - | - |
| 手机与电脑不在同一局域网 | 确保手机连接与电脑相同的 Wi-Fi 网络 |
| 防火墙拦截了端口 | 在防火墙中放行 WebSocket 服务使用的端口（默认 9999） |
| 二维码中的地址错误 | 检查二维码中的 IP 地址是否为电脑的正确局域网 IP，不要使用 `127.0.0.1` 或 `localhost` |
| 服务未正常启动 | 检查终端窗口是否有错误输出，确保服务正在运行 |
| 某些 VPN 会拦截非代理流量 | 关闭 VPN |

</details>

<details>
<summary><b>Q15: 服务启动后控制台没有输出信息</b></summary>

这是正常现象。根据官方仓库说明，服务启动后控制台不会输出任何内容。如需查看调试信息，可参考官方仓库提供的调试信息开关方式。

</details>

<details>
<summary><b>Q16: 配对失败，提示 400 或 401</b></summary>

**错误码说明**：

| 错误码 | 说明 |
| - | - |
| 400 | 此 ID 已被其他客户端绑定 |
| 401 | 要绑定的目标客户端不存在 |
| 402 | 收信方和寄信方不是绑定关系 |
| 404 | 未找到收信人（离线） |

**解决方法**：检查二维码是否正确生成了 `clientId`，确保客户端与服务端的连接未中断，并确认没有重复绑定。

</details>

<details>
<summary><b>Q17: 消息发送失败，提示 405</b></summary>

**原因**：JSON 消息长度超过了 1950 字符，APP 会丢弃该消息。

**解决方法**：将消息拆分为多条发送，确保每条消息长度在 1950 字符以内。

</details>

### 通用问题

<details>
<summary><b>Q18: 日志在 Qt 界面不显示或显示等级不对</b></summary>

**现象**：
程序运行时，首页的日志窗口没有输出，或者只显示 ERROR 级别，而配置中设置了 INFO。

**解决方案**：
- 检查配置文件中的 `app.log.ui_log_level` 值（0=DEBUG,1=INFO,2=WARN,3=ERROR,4=NONE）。例如需要显示 INFO 及以上，应设置为 1。
- 确认没有在代码中动态修改 UI 日志的 sink 级别（`DebugLog::set_log_sink_level("qt_ui", level)`）。
- 如果使用 `LOG_MODULE` 宏，确保模块名称和函数名称正确，且日志等级不低于全局设置。

</details>

<details>
<summary><b>Q19: Windows 调试控制台不出现</b></summary>

**现象**：
在配置文件中设置了 `"debug": true`，但程序启动时没有弹出黑色控制台窗口。

**解决方案**：
- 仅当编译为 Debug 配置且 `app.debug` 为 true 时才会创建控制台。Release 模式下可能被优化掉。
- 检查是否在 Windows 平台（该功能依赖 `<windows.h>`，Linux/macOS 无效）。
- 尝试以管理员身份运行程序。

</details>

---

## 十二、贡献指南

欢迎提交 Issue 和 Pull Request。在贡献前请确保：

- 代码遵循现有风格（缩进 4 空格，使用 `#pragma once`，命名规范）。更多细节请参考 [编码规范](#十编码规范)。
- 请务必使用 UTF-8 without BOM 编码提交代码。
- 添加或修改功能时更新相关文档（如 README.md）。
- 确保本地测试通过（编译通过，功能正常）。
- 对于较大的改动，请先开 Issue 讨论。

---

## 十三、许可证

本项目基于 MIT 许可证开源，详情请参阅 [LICENSE.txt](LICENSE.txt) 文件。

---

## 十四、联系方式

- 作者：[CrimsonSeraph]
- BiliBili：[浪天幽影(UID：1741002917)](https://space.bilibili.com/1741002917?spm_id_from=333.1007.0.0)
- X：[𝒞𝓇𝒾𝓂𝓈𝑜𝓃𝒮𝑒𝓇𝒶𝓅𝒽✟升天✟(@CrimSeraph_QwQ)](https://x.com/CrimSeraph_QwQ)
- 项目主页：[https://github.com/CrimsonSeraph/DG-LAB-Client](https://github.com/CrimsonSeraph/DG-LAB-Client)
