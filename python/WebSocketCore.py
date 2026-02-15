import asyncio
import json
import logging
from typing import Callable, Optional
import websockets
from websockets.exceptions import ConnectionClosed

# 配置日志模块，设置日志级别为INFO，并定义日志格式
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)  # 获取当前模块的日志记录器


class DGLabConfig:
    """DGLab客户端配置类，用于存储WebSocket连接的相关配置参数"""

    def __init__(self):
        """初始化配置参数"""
        self.ws_url = "ws://localhost:9999"  # WebSocket服务器默认地址
        self.heartbeat_interval = 60  # 心跳包发送间隔时间（秒）
        self.reconnect_delay = 5  # 连接断开后重连的延迟时间（秒）
        self.max_message_length = 1950  # 允许发送的最大消息长度（字节）


class DGLabClient:
    """DGLab客户端主类，负责管理与DGLab服务器的WebSocket通信"""

    def __init__(self):
        """初始化DGLab客户端实例"""
        self.config = DGLabConfig()  # 客户端配置实例
        self.websocket: Optional[websockets.WebSocketClientProtocol] = None  # WebSocket连接对象
        self.client_id: Optional[str] = None  # 客户端ID（由服务器分配）
        self.target_id: Optional[str] = None  # 目标设备ID（要绑定的设备）
        self.is_connected = False  # 连接状态标志
        self.on_message_callback: Optional[Callable[[dict], None]] = None  # 消息接收回调函数
        self.on_bind_callback: Optional[Callable[[str, str], None]] = None  # 绑定成功回调函数
        self.on_error_callback: Optional[Callable[[int, str], None]] = None  # 错误回调函数
        self.loop = asyncio.get_event_loop()  # 获取当前事件循环

    # ========== 配置修改方法 ==========

    def set_ws_url(self, url: str):
        """设置WebSocket服务器地址

        Args:
            url: WebSocket服务器地址，如"ws://localhost:9999"
        """
        self.config.ws_url = url

    def set_heartbeat_interval(self, interval: int):
        """设置心跳包发送间隔时间

        Args:
            interval: 心跳间隔时间（秒）
        """
        self.config.heartbeat_interval = interval

    def set_reconnect_delay(self, delay: int):
        """设置连接断开后的重连延迟时间

        Args:
            delay: 重连延迟时间（秒）
        """
        self.config.reconnect_delay = delay

    def set_on_message(self, callback: Callable[[dict], None]):
        """设置消息接收回调函数

        Args:
            callback: 回调函数，接收一个字典参数（解析后的消息数据）
        """
        self.on_message_callback = callback

    def set_on_bind(self, callback: Callable[[str, str], None]):
        """设置绑定成功回调函数

        Args:
            callback: 回调函数，接收两个参数：client_id（客户端ID）和target_id（目标设备ID）
        """
        self.on_bind_callback = callback

    def set_on_error(self, callback: Callable[[int, str], None]):
        """设置错误回调函数

        Args:
            callback: 回调函数，接收两个参数：code（错误代码）和message（错误信息）
        """
        self.on_error_callback = callback

    # ========== 连接管理 ==========

    def connect(self) -> bool:
        """
        同步接口：
        1. 尝试建立一次性连接并等待 client_id
        2. 后台继续心跳和消息接收循环
        """
        # 如果已经连接且有client_id，直接返回True
        if self.is_connected and self.client_id is not None:
            return True

        # 第一次连接并等待client_id
        first_result = self.loop.run_until_complete(self._connect_once())

        # 启动后台循环任务（不阻塞 C++）
        # 如果后台任务已经存在，也不会重复启动
        if not hasattr(self, "_background_task") or self._background_task.done():
            self._background_task = self.loop.create_task(self._heartbeat_loop())

        return first_result

    async def _connect_once(self) -> bool:
        """
        尝试建立一次连接，并等待服务端发送 client_id。
        返回 True 表示第一次成功获取 client_id。
        """
        try:
            logger.info(f"尝试连接到 {self.config.ws_url}")
            self.websocket = await websockets.connect(self.config.ws_url)
            self.is_connected = True

            # 等待第一次 client_id
            timeout = 10
            while self.client_id is None:
                try:
                    message = await asyncio.wait_for(self.websocket.recv(), timeout=timeout)
                    data = json.loads(message)
                    if data.get("type") == "bind" and data.get("message") == "targetId":
                       self.client_id = data.get("clientId")
                       logger.info(f"收到 client_id: {self.client_id}")
                       break
                except asyncio.TimeoutError:
                    logger.warning("等待 client_id 超时")
                    return False
                except Exception as e:
                    logger.error(f"等待 client_id 时错误: {e}")
                    return False

            return True

        except Exception as e:
            logger.error(f"第一次连接失败: {e}")
        self.is_connected = False
        self.websocket = None
        return False

    async def _wait_for_client_id(self):
        """等待服务器发送bind消息以获取客户端ID

        服务器连接成功后，会发送一个type为"bind"的消息，其中包含分配的client_id
        此方法会持续监听直到收到client_id或超时
        """
        while self.client_id is None:
            try:
                # 等待服务器消息，设置10秒超时
                message = await asyncio.wait_for(self.websocket.recv(), timeout=10)
                data = json.loads(message)  # 解析JSON消息

                # 检查是否为bind类型消息且message为"targetId"
                if data.get("type") == "bind" and data.get("message") == "targetId":
                    self.client_id = data.get("clientId")
                    logger.info(f"收到 client_id: {self.client_id}")
            except asyncio.TimeoutError:
                logger.warning("等待 client_id 超时")
            except Exception as e:
                logger.error(f"等待 client_id 时错误: {e}")

    # ========== 心跳机制 ==========

    async def _heartbeat_loop(self):
        """心跳循环任务，定期向服务器发送心跳包以保持连接活跃"""
        while self.is_connected:
            await self.send_heartbeat()  # 发送心跳包
            await asyncio.sleep(self.config.heartbeat_interval)  # 等待指定间隔

    # ========== 消息接收处理 ==========

    async def _receive_loop(self):
        """消息接收循环任务，持续接收并处理服务器发送的消息"""
        while self.is_connected:
            try:
                # 接收原始消息
                message = await self.websocket.recv()
                data = json.loads(message)  # 解析为JSON
                self._handle_received_message(data)  # 处理消息
            except ConnectionClosed:
                # 连接断开，退出循环
                break
            except json.JSONDecodeError:
                # 消息格式错误
                logger.warning("收到无效 JSON")
            except Exception as e:
                # 其他接收错误
                logger.error(f"接收错误: {e}")

    def _handle_received_message(self, data: dict):
        """处理接收到的消息，根据消息类型执行不同操作

        Args:
            data: 解析后的消息字典
        """
        msg_type = data.get("type")  # 消息类型
        message = data.get("message")  # 消息内容

        # 处理bind类型消息（绑定相关）
        if msg_type == "bind":
            if message == "200":
                # 绑定成功，记录target_id
                self.target_id = data.get("targetId")
                logger.info(f"绑定成功: target_id = {self.target_id}")
                # 调用绑定成功回调
                if self.on_bind_callback:
                    self.on_bind_callback(self.client_id, self.target_id)
            else:
                # 绑定失败，解析错误代码和信息
                code = int(message) if message.isdigit() else 0
                err_msg = self._get_error_message(code)
                logger.error(f"绑定错误: {code} - {err_msg}")
                # 调用错误回调
                if self.on_error_callback:
                    self.on_error_callback(code, err_msg)

        # 处理error类型消息（服务器错误）
        elif msg_type == "error":
            code = int(message) if message.isdigit() else 500
            err_msg = self._get_error_message(code)
            logger.error(f"服务器错误: {code} - {err_msg}")
            if self.on_error_callback:
                self.on_error_callback(code, err_msg)

        # 处理msg类型消息（常规消息）
        elif msg_type == "msg":
            # 强度同步消息
            if message.startswith("strength-"):
                logger.info(f"收到强度更新: {message}")
            # 设备反馈消息
            elif message.startswith("feedback-"):
                logger.info(f"收到反馈: {message}")

        # 处理break类型消息（断开指令）
        elif msg_type == "break":
            logger.info("收到断开指令")
            self.is_connected = False

        # 调用用户设置的消息回调函数
        if self.on_message_callback:
            self.on_message_callback(data)

    # ========== 消息发送 ==========

    async def send(self, data: dict):
        """发送JSON格式的消息到服务器，并检查消息长度

        Args:
            data: 要发送的消息字典

        Returns:
            bool: 发送成功返回True，失败返回False
        """
        # 检查连接状态
        if not self.is_connected or not self.websocket:
            logger.warning("未连接，无法发送")
            return False

        # 将字典转换为JSON字符串
        json_str = json.dumps(data)

        # 检查消息长度是否超出限制
        if len(json_str) > self.config.max_message_length:
            logger.error("消息过长，无法发送")
            return False

        try:
            # 发送消息
            await self.websocket.send(json_str)
            logger.debug(f"已发送: {json_str}")
            return True
        except Exception as e:
            logger.error(f"发送失败: {e}")
            return False

    async def send_heartbeat(self):
        """发送心跳包到服务器

        心跳包格式：
        {
            "type": "heartbeat",
            "clientId": <客户端ID>,
            "targetId": <目标设备ID>,
            "message": "200"
        }
        """
        if not self.client_id:
            return

        data = {
            "type": "heartbeat",
            "clientId": self.client_id,
            "targetId": self.target_id or "",  # 如果没有target_id则发送空字符串
            "message": "200"  # 心跳消息固定为"200"
        }
        await self.send(data)

    # ========== 设备控制方法 ==========

    async def bind_target(self, target_id: str):
        """绑定目标设备

        Args:
            target_id: 要绑定的目标设备ID

        绑定请求格式：
        {
            "type": "bind",
            "clientId": <客户端ID>,
            "targetId": <目标设备ID>,
            "message": "DGLAB"
        }
        """
        if not self.client_id:
            logger.warning("无 client_id，无法绑定")
            return

        self.target_id = target_id
        data = {
            "type": "bind",
            "clientId": self.client_id,
            "targetId": target_id,
            "message": "DGLAB"  # 绑定消息固定为"DGLAB"
        }
        await self.send(data)

    async def send_strength_operation(self, channel: int, mode: int, value: int):
        """发送强度控制命令

        Args:
            channel: 通道编号（1=A通道，2=B通道）
            mode: 操作模式（0=减少，1=增加，2=设置为指定值）
            value: 强度值（0-200）

        消息格式："strength-<通道>+<模式>+<值>"
        例如：strength-1+2+100 表示将A通道强度设置为100
        """
        # 参数验证
        if channel not in (1, 2) or mode not in (0, 1, 2) or not 0 <= value <= 200:
            logger.error("无效参数")
            return

        message = f"strength-{channel}+{mode}+{value}"
        data = {
            "type": "msg",
            "clientId": self.client_id,
            "targetId": self.target_id,
            "message": message
        }
        await self.send(data)

    async def send_pulse(self, channel: str, pulses: list[str]):
        """发送波形数据到设备

        Args:
            channel: 通道标识（'A'或'B'）
            pulses: 波形数据列表，每个元素是8字节的HEX字符串，最多100个

        消息格式："pulse-<通道>:<JSON格式的波形列表>"
        例如：pulse-A:["00000000","FFFFFFFF",...]
        """
        # 参数验证
        if channel not in ('A', 'B') or len(pulses) > 100:
            logger.error("无效参数")
            return

        # 构造波形消息
        message = f'pulse-{channel}:{json.dumps(pulses)}'
        data = {
            "type": "msg",
            "clientId": self.client_id,
            "targetId": self.target_id,
            "message": message
        }
        await self.send(data)

    async def send_clear_queue(self, channel: int):
        """清空指定通道的波形队列

        Args:
            channel: 通道编号（1=A通道，2=B通道）

        消息格式："clear-<通道>"
        例如：clear-1 表示清空A通道波形队列
        """
        if channel not in (1, 2):
            logger.error("无效通道")
            return

        message = f"clear-{channel}"
        data = {
            "type": "msg",
            "clientId": self.client_id,
            "targetId": self.target_id,
            "message": message
        }
        await self.send(data)

    # ========== 工具方法 ==========

    def generate_qr_content(self):
        """生成用于DGLab App扫描的二维码内容

        Returns:
            str: 二维码内容字符串，格式为：
                 "https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#<服务器地址>/<客户端ID>"
                 如果client_id不存在则返回None
        """
        if not self.client_id:
            return None

        # 提取服务器地址（去掉协议头）
        server_address = self.config.ws_url.split('://')[1]
        return f"https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#{server_address}/{self.client_id}"

    @staticmethod
    def _get_error_message(code: int) -> str:
        """根据错误代码获取对应的错误描述

        Args:
            code: 错误代码

        Returns:
            str: 错误描述文本
        """
        # 错误代码与描述的映射表
        errors = {
            200: "成功",
            209: "对方客户端已断开",
            210: "二维码中没有有效的 clientID",
            211: "socket 连接上了，但服务器迟迟不下发 app 端的 id 来绑定",
            400: "此 id 已被其他客户端绑定关系",
            401: "要绑定的目标客户端不存在",
            402: "收信方和寄信方不是绑定关系",
            403: "发送的内容不是标准 json 对象",
            404: "未找到收信人（离线）",
            405: "下发的 message 长度大于 1950",
            500: "服务器内部异常"
        }
        return errors.get(code, "未知错误")

    async def close(self):
        """主动关闭WebSocket连接"""
        if self.websocket:
            await self.websocket.close()
        self.is_connected = False
