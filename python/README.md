# Python 模块目录（python）

本目录存放与 DGLab 客户端交互的 Python 模块，采用 **TCP 服务器 + WebSocket 客户端** 的双层架构：  
`Bridge.py` 作为独立进程启动，监听本地 TCP 端口，接收来自 C++ 主程序的 JSON 命令，并调用 `WebSocketCore.py` 中的 `DGLabClient` 与 DGLab 服务器进行 WebSocket 通信，实现对设备的控制。

---

## 文件说明

### `Bridge.py`

**主入口脚本**，负责启动异步 TCP 服务器，处理与 C++ 客户端的命令交互，并管理 `DGLabClient` 实例。

#### 主要功能
- 启动 TCP 服务器（绑定 `127.0.0.1`，随机端口），将端口号打印到标准输出供 C++ 客户端读取。
- 接收 C++ 客户端发送的 JSON 命令，解析后调用 `DGLabClient` 的对应方法。
- 将执行结果以 JSON 格式返回给 C++ 客户端（每条响应后附加换行符作为消息边界）。
- 支持的命令：`connect`、`close`、`set_ws_url`、`bind_target`、`send_strength`、`get_qr` 等。
- 当 C++ 客户端断开连接时，自动清理资源并等待新连接。

#### 依赖
- Python 3.9+
- `asyncio`（标准库）
- `websockets`（需安装）

#### 使用方式
由 C++ 主程序通过 `QProcess` 启动，脚本启动后立即打印端口号，之后通过该端口建立 TCP 连接进行通信。

---

### `WebSocketCore.py`

**DGLab WebSocket 客户端核心库**，封装了与 DGLab 服务器的 WebSocket 连接、消息收发、设备绑定、强度控制等底层逻辑，并提供同步包装方法以便在非异步环境中调用。

#### 主要功能
- **配置管理**：设置 WebSocket 服务器地址、心跳间隔、重连延迟、消息长度限制等。
- **连接管理**：`connect()`（同步）和 `connect_async()`（异步）方法，自动完成首次连接并等待 `client_id`，同时启动后台心跳与消息接收循环。
- **消息处理**：接收服务器消息后按类型（`bind`、`error`、`msg`、`break`）分发，并触发用户注册的回调函数。
- **设备控制**：绑定目标设备（`bind_target`）、发送强度调节（`send_strength_operation`）、发送波形数据（`send_pulse`）、清空队列（`send_clear_queue`）等，均提供同步与异步版本。
- **工具方法**：生成二维码内容（`generate_qr_content`）、错误码解析（`_get_error_message`）等。

#### 依赖
- Python 3.9+
- `websockets` 库

#### 注意事项
- 该类基于 `asyncio` 实现，但通过 `run_until_complete` 包装了同步接口（如 `connect`、`sync_send_strength_operation`、`sync_close`），便于在非异步环境中直接调用。
- `Bridge.py` 内部使用异步方式调用此类的方法，以充分利用 `asyncio` 的事件循环。

---

## 整体工作流程

1. C++ 主程序启动 Python 子进程执行 `Bridge.py`。
2. `Bridge.py` 启动 TCP 服务器并输出端口号。
3. C++ 通过 `QTcpSocket` 连接到该端口，发送 JSON 命令（如 `{"cmd":"connect","req_id":123}`）。
4. `Bridge.py` 解析命令，调用 `DGLabClient` 的对应方法，等待结果。
5. `DGLabClient` 与远程 DGLab WebSocket 服务器交互，返回结果给 `Bridge.py`。
6. `Bridge.py` 将结果（如 `{"status":"ok","req_id":123}`）通过 TCP 返回给 C++。
7. C++ 根据返回更新界面或继续下一步操作。

---

## 编译与运行

- 确保 Python 环境已安装 `websockets` 库：
  ```bash
  pip install websockets
  ```
- C++ 主程序启动时需正确配置 Python 解释器路径及 `Bridge.py` 的路径（见 `AppConfig` 中的 `python.path` 和 `python.bridge_path` 配置项）。
- 运行期间，Python 子进程的日志会输出到标准错误，可通过 C++ 捕获或重定向查看。
