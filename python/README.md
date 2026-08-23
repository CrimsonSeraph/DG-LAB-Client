# Python 模块目录（python）

本目录存放与 DGLab 客户端交互的 Python 模块，采用 **TCP 服务器 + WebSocket 客户端** 的双层架构: `Bridge.py` 作为独立进程启动，监听本地 TCP 端口，接收来自 C++ 主程序的 JSON 命令，并调用 `WebSocketCore.py` 中的 `DGLabClient` 与 DGLab 服务器进行 WebSocket 通信，实现对设备的控制。

此外，本目录还包含一个通用的路径查找工具 `PathFinder.py`，可供 C++ 主程序或其他模块调用，用于定位文件或目录（例如 Steam 游戏安装路径）。

---

## 文件说明

### `Bridge.py`

**主入口脚本**，负责启动异步 TCP 服务器，处理与 C++ 客户端的命令交互，并管理 `DGLabClient` 实例。

#### 主要功能

- 启动 TCP 服务器（绑定 `127.0.0.1`，随机端口），将端口号打印到标准输出供 C++ 客户端读取。
- 接收 C++ 客户端发送的 JSON 命令，解析后调用 `DGLabClient` 的对应方法。
- 将执行结果以 JSON 格式返回给 C++ 客户端（每条响应后附加换行符作为消息边界）。
- 主动消息推送: 当从 DGLab WebSocket 服务器收到任何消息（如绑定结果、强度更新、反馈、错误码、断开指令等）时，会立即通过 TCP 连接主动发送一条 JSON 给 Qt 客户端，格式为 `{"type": "active_message", "data": <原始消息对象>}`。Qt 客户端应持续监听并处理这些消息，以便实时更新界面或执行相应逻辑。
- 支持的命令（`cmd` 字段）:
    - **连接管理**: `connect`、`close`、`set_ws_url`
    - **绑定与状态查询**: `bind_target`、`get_client_id`、`get_target_id`、`get_connection_status`
    - **强度控制**: `send_strength`（mode: 0=减少,1=增加,2=设置指定值,3=连续减少,4=连续增加）
    - **波形发送**: `send_pulse`（支持脉冲列表，channel 可为 1/2 或 'A'/'B'）
    - **队列清空**: `clear_queue`
    - **二维码生成**: `get_qr_path`
    - **日志级别**: `set_log_level`（支持 DEBUG/INFO/WARNING/ERROR）
- 当 C++ 客户端断开连接时，自动清理资源并等待新连接。

#### 依赖

- Python 3.9+
- `asyncio`（标准库）
- `websockets`（需安装）
- `qrcode`（需安装）

#### 使用方式

由 C++ 主程序通过 `QProcess` 启动，脚本启动后立即打印端口号，之后通过该端口建立 TCP 连接进行通信。

---

### `WebSocketCore.py`

**DGLab WebSocket 客户端核心库**，封装了与 DGLab 服务器的 WebSocket 连接、消息收发、设备绑定、强度控制等底层逻辑，完全遵循 **v2 后端协议**，并提供同步包装方法以便在非异步环境中调用。

#### 主要功能

- **配置管理**: 设置 WebSocket 服务器地址、心跳间隔、重连延迟、消息长度限制等。
- **连接管理**: `connect()`（同步）和 `connect_async()`（异步）方法，自动完成首次连接并等待 `client_id`，同时启动后台心跳与消息接收循环。
- **消息处理**: 接收服务器消息后按类型（`bind`、`error`、`msg`、`break`）分发，并触发用户注册的回调函数。
- **设备控制**:
    - 绑定目标设备（`bind_target`）
    - 强度调节（`send_strength_operation`，支持减少/增加/设置指定值）
    - 发送波形数据（`send_pulse`，支持批量脉冲列表，channel 为 'A'/'B'）
    - 清空队列（`send_clear_queue`，channel 为 1/2）
- **工具方法**: 生成二维码内容（`generate_qr_content`）、保存二维码图片（`get_qr`）、错误码解析（`_get_error_message`）等。
- **同步/异步双接口**: 所有核心方法均提供同步版本（如 `sync_send_strength_operation`）和异步版本，方便在不同环境中调用。

#### 依赖

- Python 3.9+
- `websockets`
- `qrcode`

#### 注意事项

- 该类基于 `asyncio` 实现，但通过 `run_until_complete` 包装了同步接口，便于在非异步环境中直接调用。
- `Bridge.py` 内部使用异步方式调用此类的方法，以充分利用 `asyncio` 的事件循环。
- 协议版本: v2（参考 DGLab 官方文档），消息类型 `type` 字段: `1`（减少强度）、`2`（增加强度）、`3`（设置强度）、`4`（清空队列）、`clientMsg`（波形发送）等。

---

### `PathFinder.py`

**跨平台文件/目录路径查找工具**，提供统一的接口在 Windows、macOS 和 Linux 上查找指定的文件或目录，特别支持通过 Steam 库定位游戏安装路径，也支持手动指定目录进行搜索。

#### 主要功能

- **双模式查找**:
    - `steam` 模式：自动检测 Steam 安装目录，解析所有库文件夹，查找匹配的游戏名称，并返回游戏目录或可执行文件路径。
    - `manual` 模式：支持直接指定路径（文件或目录），或在指定目录下搜索特定名称的文件/目录。
- **交互控制**:
    - 可启用图形化交互（基于 `tkinter`）：
        - 多个匹配项时弹出选择对话框。
        - 未找到匹配项时可手动输入路径。
        - 当 `manual_path` 为空字符串且交互开启时，弹出系统文件/目录选择器（根据 `target_type`）。
    - 支持非交互模式（`interactive=False`），直接返回第一个匹配或 `None`。
- **日志系统**:
    - 内置日志支持，级别可调（DEBUG/INFO/WARNING/ERROR/NONE），每条日志均记录模块名、函数名和级别。
- **跨平台兼容**:
    - Windows：读取注册表获取 Steam 路径，并支持常见默认安装目录。
    - macOS：检测 `/Applications/Steam.app`。
    - Linux：检测 `~/.steam/steam` 或 `~/.local/share/Steam`。

#### 核心 API

**类**: `PathFinder` **方法**: `find_path(self, mode, steam_game=None, manual_path=None, search_name=None, target_type='file', interactive=True, executable_names=None) -> Optional[str]`

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | str | `'steam'` 或 `'manual'` |
| `steam_game` | Optional[str] | Steam 游戏名称（如 `"Counter-Strike Global Offensive"`） |
| `manual_path` | Optional[str] | 手动指定的路径（文件或目录），若为 `""` 且交互模式则弹出选择对话框 |
| `search_name` | Optional[str] | 在 `manual_path` 目录下搜索的具体文件名或目录名 |
| `target_type` | str | `'file'` 或 `'directory'`，期望返回的类型 |
| `interactive` | bool | 是否启用交互（弹窗/提示） |
| `executable_names` | Optional[List[str]] | 在 Steam 游戏目录中查找可执行文件时的名称列表（默认根据平台自动设置） |

**返回值**: 匹配的路径字符串，若未找到且不交互则返回 `None`。

#### 命令行接口

```bash
python PathFinder.py --mode steam --steam-game "Counter-Strike Global Offensive" --type file
python PathFinder.py --mode manual --manual-path "C:\Games" --search-name "cs2.exe" --type file
python PathFinder.py --mode manual --manual-path "" --type file  # 弹出选择文件对话框
```

可选参数: `--search-name`, `--type`, `--no-interactive`, `--executable-names`, `--log-level`。

#### 作为模块使用

```python
from PathFinder import find_path   # 模块级函数（兼容旧接口）或 PathFinder 类

path = find_path(
    mode='steam',
    steam_game='Counter-Strike Global Offensive',
    target_type='file',
    interactive=False
)
if path:
    print(f"找到路径: {path}")
else:
    print("未找到")
```

#### 依赖

- Python 3.6+
- 标准库: `os`, `sys`, `platform`, `re`, `logging`, `argparse`, `typing`
- 可选: `tkinter`（图形化交互，通常随 Python 一起安装）
- 无需额外第三方库（不使用 `vdf` 解析库，内置正则解析 `libraryfolders.vdf`）

#### 注意事项

- 该模块不涉及任何网络或 WebSocket 通信，纯本地文件系统操作。
- 在非交互模式下，不会弹出任何窗口，适合在后台或自动化脚本中使用。
- 日志默认输出到控制台，可通过 `set_log_level` 调整级别。

---

## 整体工作流程

1. C++ 主程序启动 Python 子进程执行 `Bridge.py`。
2. `Bridge.py` 启动 TCP 服务器并输出端口号。
3. C++ 通过 `QTcpSocket` 连接到该端口，发送 JSON 命令（如 `{"cmd":"connect","req_id":123}`）。
4. `Bridge.py` 解析命令，调用 `DGLabClient` 的对应方法，等待结果。
5. `DGLabClient` 与远程 DGLab WebSocket 服务器交互，返回结果给 `Bridge.py`。
6. `Bridge.py` 将结果（如 `{"status":"ok","req_id":123}`）通过 TCP 返回给 C++。
7. 此后，每当 DGLab 服务器主动推送消息（如强度变化、绑定结果、错误等），`Bridge.py` 会立即通过同一 TCP 连接主动发送 `{"type":"active_message","data":...}` 给 C++。C++ 应保持连接打开并持续读取数据，以处理这些实时推送。
8. C++ 根据返回更新界面或继续下一步操作。

另外，C++ 主程序也可直接调用 `PathFinder.py`（作为独立工具或导入模块）来获取路径信息，例如定位 Steam 游戏目录或配置文件。

---

## 编译与运行

- 确保 Python 环境已安装 `websockets` 与 `qrcode` 库:
    ```bash
    pip install websockets
    pip install qrcode[pil]
    ```
- C++ 主程序启动时需正确配置 Python 解释器路径及 `Bridge.py` 的路径（见 `AppConfig` 中的 `python.path` 和 `python.bridge_path` 配置项）。
- 运行期间，Python 子进程的日志会输出到标准错误，可通过 C++ 捕获或重定向查看。

- `PathFinder.py` 无需额外安装依赖，直接运行或导入即可。

---

## 通信协议示例

### 请求（C++ → Bridge.py）

```json
{ "cmd": "send_strength", "channel": 1, "mode": 2, "value": 80, "req_id": 1001 }
```

### 响应（Bridge.py → C++）

```json
{ "status": "ok", "message": "强度指令已发送", "req_id": 1001 }
```

### 主动推送消息（Bridge.py → C++，来自 WebSocket 服务器）

当 WebSocket 服务器发送消息时，Bridge.py 会主动推送如下格式:

**绑定成功示例: **

```json
{ "type": "active_message", "data": { "type": "bind", "message": "200", "targetId": "xxx" } }
```

**强度更新示例: **

```json
{ "type": "active_message", "data": { "type": "msg", "message": "strength-A:80" } }
```

**错误示例: **

```json
{ "type": "active_message", "data": { "type": "error", "message": "401" } }
```

**断开指令示例: **

```json
{ "type": "active_message", "data": { "type": "break" } }
```

---

## 支持的命令速查表

| cmd | 必需参数 | 可选参数 | 说明 |
| --- | --- | --- | --- |
| `connect` | 无 | - | 建立 WebSocket 连接 |
| `close` | 无 | - | 断开 WebSocket 连接 |
| `set_ws_url` | `url` | - | 设置 WebSocket 服务器地址 |
| `bind_target` | `target_id` | - | 绑定目标设备 |
| `get_client_id` | 无 | - | 获取当前 clientId |
| `get_target_id` | 无 | - | 获取当前 targetId |
| `get_connection_status` | 无 | - | 获取连接状态详情 |
| `send_strength` | `channel`, `mode` | `value` | mode 0/1/2 时 value 可选（连续模式时 value 为次数） |
| `send_pulse` | `channel`, `pulses` | `duration` | pulses 为字符串数组（8字节HEX） |
| `clear_queue` | `channel` | - | 清空指定通道波形队列 |
| `get_qr_path` | 无 | - | 生成二维码并返回文件路径 |
| `set_log_level` | `level` | - | 设置日志级别（DEBUG/INFO/WARNING/ERROR） |

---

## 注意事项

- `Bridge.py` 中的 `channel` 参数接受 `1`、`2` 或 `'A'`、`'B'`、`'a'`、`'b'`，内部会统一转换。
- `send_strength` 的连续增减模式（mode=3/4）会自动循环发送，最多执行 100 次。
- `send_pulse` 的 `pulses` 列表最多 100 个元素，超出会被截断。
- 所有响应均包含 `req_id` 字段（若请求中包含），用于请求-响应对应。
- 主动推送消息没有 `req_id`，通过 `type` 字段区分；C++ 客户端必须持续读取 TCP 数据，不能仅按请求-响应模式处理。
- `PathFinder.py` 在交互模式下会尝试使用 `tkinter`，若环境不支持 `tkinter` 会自动回退到控制台交互。
