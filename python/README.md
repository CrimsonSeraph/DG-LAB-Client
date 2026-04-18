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
- 主动消息推送：当从 DGLab WebSocket 服务器收到任何消息（如绑定结果、强度更新、反馈、错误码、断开指令等）时，会立即通过 TCP 连接主动发送一条 JSON 给 Qt 客户端，格式为 `{"type": "active_message", "data": <原始消息对象>}`。Qt 客户端应持续监听并处理这些消息，以便实时更新界面或执行相应逻辑。
- 支持的命令（`cmd` 字段）：
  - **连接管理**：`connect`、`close`、`set_ws_url`
  - **绑定与状态查询**：`bind_target`、`get_client_id`、`get_target_id`、`get_connection_status`
  - **强度控制**：`send_strength`（mode: 0=减少,1=增加,2=设置指定值,3=连续减少,4=连续增加）
  - **波形发送**：`send_pulse`（支持脉冲列表，channel 可为 1/2 或 'A'/'B'）
  - **队列清空**：`clear_queue`
  - **二维码生成**：`get_qr_path`
  - **日志级别**：`set_log_level`（支持 DEBUG/INFO/WARNING/ERROR）
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
- **配置管理**：设置 WebSocket 服务器地址、心跳间隔、重连延迟、消息长度限制等。
- **连接管理**：`connect()`（同步）和 `connect_async()`（异步）方法，自动完成首次连接并等待 `client_id`，同时启动后台心跳与消息接收循环。
- **消息处理**：接收服务器消息后按类型（`bind`、`error`、`msg`、`break`）分发，并触发用户注册的回调函数。
- **设备控制**：
  - 绑定目标设备（`bind_target`）
  - 强度调节（`send_strength_operation`，支持减少/增加/设置指定值）
  - 发送波形数据（`send_pulse`，支持批量脉冲列表，channel 为 'A'/'B'）
  - 清空队列（`send_clear_queue`，channel 为 1/2）
- **工具方法**：生成二维码内容（`generate_qr_content`）、保存二维码图片（`get_qr`）、错误码解析（`_get_error_message`）等。
- **同步/异步双接口**：所有核心方法均提供同步版本（如 `sync_send_strength_operation`）和异步版本，方便在不同环境中调用。

#### 依赖
- Python 3.9+
- `websockets`
- `qrcode`

#### 注意事项
- 该类基于 `asyncio` 实现，但通过 `run_until_complete` 包装了同步接口，便于在非异步环境中直接调用。
- `Bridge.py` 内部使用异步方式调用此类的方法，以充分利用 `asyncio` 的事件循环。
- 协议版本：v2（参考 DGLab 官方文档），消息类型 `type` 字段：`1`（减少强度）、`2`（增加强度）、`3`（设置强度）、`4`（清空队列）、`clientMsg`（波形发送）等。

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

---

## 编译与运行

- 确保 Python 环境已安装 `websockets` 与 `qrcode` 库：
  ```bash
  pip install websockets
  pip install qrcode[pil]
  ```
- C++ 主程序启动时需正确配置 Python 解释器路径及 `Bridge.py` 的路径（见 `AppConfig` 中的 `python.path` 和 `python.bridge_path` 配置项）。
- 运行期间，Python 子进程的日志会输出到标准错误，可通过 C++ 捕获或重定向查看。

---

## 通信协议示例

### 请求（C++ → Bridge.py）
```json
{"cmd":"send_strength", "channel":1, "mode":2, "value":80, "req_id":1001}
```

### 响应（Bridge.py → C++）
```json
{"status":"ok", "message":"强度指令已发送", "req_id":1001}
```

### 主动推送消息（Bridge.py → C++，来自 WebSocket 服务器）
当 WebSocket 服务器发送消息时，Bridge.py 会主动推送如下格式：

**绑定成功示例：**
```json
{"type":"active_message","data":{"type":"bind","message":"200","targetId":"xxx"}}
```

**强度更新示例：**
```json
{"type":"active_message","data":{"type":"msg","message":"strength-A:80"}}
```

**错误示例：**
```json
{"type":"active_message","data":{"type":"error","message":"401"}}
```

**断开指令示例：**
```json
{"type":"active_message","data":{"type":"break"}}
```

---

## 支持的命令速查表

| cmd | 必需参数 | 可选参数 | 说明 |
| - | - | - | - |
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
