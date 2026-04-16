"""
    Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
    SPDX-License-Identifier: GPL-3.0-only
"""

import asyncio
import json
import logging
import os
from typing import Callable, Optional

import qrcode
import websockets
from websockets.exceptions import ConnectionClosed

# 配置日志模块，设置日志级别为INFO，并定义日志格式
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)    # 获取当前模块的日志记录器


class DGLabConfig:
    """DGLab客户端配置类，用于存储WebSocket连接的相关配置参数"""

    def __init__(self):
        """初始化配置参数"""
        self.ws_url = "ws://localhost:9999" # WebSocket服务器默认地址
        self.heartbeat_interval = 60    # 心跳包发送间隔时间（秒）
        self.reconnect_delay = 5    # 连接断开后重连的延迟时间（秒）
        self.max_message_length = 1950  # 允许发送的最大消息长度（字节）
        self.is_connect = False


class DGLabClient:
    """
    DGLab客户端主类，负责管理与DGLab服务器的WebSocket通信。
    遵循 v2 后端协议，支持强度调节、波形发送、队列清空等功能。
    """

    def __init__(self):
        """初始化DGLab客户端实例"""
        self.config = DGLabConfig()
        self.websocket: Optional[websockets.WebSocketClientProtocol] = None
        self.client_id: Optional[str] = None    # 由服务器分配的客户端ID
        self.target_id: Optional[str] = None    # 绑定的目标设备ID
        self.is_connected = False   # WebSocket连接状态
        self.on_message_callback: Optional[Callable[[dict], None]] = None
        self.on_bind_callback: Optional[Callable[[str, str], None]] = None
        self.on_error_callback: Optional[Callable[[int, str], None]] = None

        # 获取或为当前线程创建一个事件循环
        try:
            self.loop = asyncio.get_event_loop()
        except RuntimeError:
            self.loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self.loop)

    # ========== 配置修改方法 ==========

    def set_ws_url(self, url: str):
        """设置WebSocket服务器地址"""
        logger.debug(f"[DGLabClient] <set_ws_url> (LOG_DEBUG): 设置 WebSocket URL: {url}")
        self.config.ws_url = url

    def set_heartbeat_interval(self, interval: int):
        """设置心跳包发送间隔时间（秒）"""
        logger.debug(f"[DGLabClient] <set_heartbeat_interval> (LOG_DEBUG): 设置心跳间隔: {interval}秒")
        self.config.heartbeat_interval = interval

    def set_reconnect_delay(self, delay: int):
        """设置重连延迟（秒）"""
        logger.debug(f"[DGLabClient] <set_reconnect_delay> (LOG_DEBUG): 设置重连延迟: {delay}秒")
        self.config.reconnect_delay = delay

    def set_on_message(self, callback: Callable[[dict], None]):
        """设置消息接收回调函数"""
        self.on_message_callback = callback

    def set_on_bind(self, callback: Callable[[str, str], None]):
        """设置绑定成功回调函数"""
        self.on_bind_callback = callback

    def set_on_error(self, callback: Callable[[int, str], None]):
        """设置错误回调函数"""
        self.on_error_callback = callback

    # ========== 连接管理 ==========

    async def connect_async(self) -> bool:
        """异步连接，等待 client_id 并启动后台任务"""
        if self.is_connected and self.client_id:
            logger.debug("[DGLabClient] <connect_async> (LOG_DEBUG): 已经连接且拥有 client_id，无需重复连接")
            return True
        logger.info("[DGLabClient] <connect_async> (LOG_INFO): 开始建立 WebSocket 连接...")
        success = await self._connect_once()
        if success:
            # 启动后台心跳和接收循环
            asyncio.create_task(self._heartbeat_loop())
            asyncio.create_task(self._receive_loop())
            self.is_connected = True
            logger.info("[DGLabClient] <connect_async> (LOG_INFO): WebSocket 连接成功，后台循环已启动")
        else:
            logger.error("[DGLabClient] <connect_async> (LOG_ERROR): WebSocket 连接失败")
        return success

    def connect(self) -> bool:
        """同步接口的 connect（不推荐使用，建议使用 connect_async）"""
        if self.is_connected:
            return True
        if self.is_connected and self.client_id is not None:
            return True
        first_result = self.loop.run_until_complete(self._connect_once())
        if not hasattr(self, "_background_task") or self._background_task.done():
            self._background_task = self.loop.create_task(self._heartbeat_loop())
        return first_result

    async def _connect_once(self) -> bool:
        """
        尝试建立一次连接，并等待服务端发送 client_id。
        返回 True 表示成功获取 client_id。
        """
        try:
            logger.info(f"[DGLabClient] <_connect_once> (LOG_INFO): 尝试连接到 {self.config.ws_url}")
            self.websocket = await websockets.connect(self.config.ws_url)
            self.is_connected = True
            logger.debug("[DGLabClient] <_connect_once> (LOG_DEBUG): WebSocket 传输层已连接，等待 client_id...")

            # 等待第一次 client_id（超时10秒）
            timeout = 10
            while self.client_id is None:
                try:
                    message = await asyncio.wait_for(self.websocket.recv(), timeout=timeout)
                    data = json.loads(message)
                    logger.debug(f"[DGLabClient] <_connect_once> (LOG_DEBUG): 收到初始消息: {data}")
                    if data.get("type") == "bind" and data.get("message") == "targetId":
                        self.client_id = data.get("clientId")
                        logger.info(f"[DGLabClient] <_connect_once> (LOG_INFO): 收到 client_id: {self.client_id}")
                        break
                except asyncio.TimeoutError:
                    logger.warning("[DGLabClient] <_connect_once> (LOG_WARN): 等待 client_id 超时")
                    return False
                except Exception as e:
                    logger.error(f"[DGLabClient] <_connect_once> (LOG_ERROR): 等待 client_id 时错误: {e}")
                    return False
            return True
        except Exception as e:
            logger.error(f"[DGLabClient] <_connect_once> (LOG_ERROR): 第一次连接失败: {e}")
        self.is_connected = False
        self.websocket = None
        return False

    async def _wait_for_client_id(self):
        """等待服务器发送bind消息以获取客户端ID（备用方法）"""
        while self.client_id is None:
            try:
                message = await asyncio.wait_for(self.websocket.recv(), timeout=10)
                data = json.loads(message)
                if data.get("type") == "bind" and data.get("message") == "targetId":
                    self.client_id = data.get("clientId")
                    logger.info(f"[DGLabClient] <_wait_for_client_id> (LOG_INFO): 收到 client_id: {self.client_id}")
            except asyncio.TimeoutError:
                logger.warning("[DGLabClient] <_wait_for_client_id> (LOG_WARN): 等待 client_id 超时")
            except Exception as e:
                logger.error(f"[DGLabClient] <_wait_for_client_id> (LOG_ERROR): 等待 client_id 时错误: {e}")

    # ========== 心跳机制 ==========

    async def _heartbeat_loop(self):
        """心跳循环任务，定期向服务器发送心跳包以保持连接活跃"""
        while self.is_connected:
            await self.send_heartbeat()
            await asyncio.sleep(self.config.heartbeat_interval)

    # ========== 消息接收处理 ==========

    async def _receive_loop(self):
        """消息接收循环任务，持续接收并处理服务器发送的消息"""
        while self.is_connected:
            try:
                message = await self.websocket.recv()
                data = json.loads(message)
                logger.debug(f"[DGLabClient] <_receive_loop> (LOG_DEBUG): 收到消息: {data}")
                self._handle_received_message(data)
            except ConnectionClosed:
                logger.warning("[DGLabClient] <_receive_loop> (LOG_WARN): WebSocket 连接已关闭")
                break
            except json.JSONDecodeError:
                logger.warning("[DGLabClient] <_receive_loop> (LOG_WARN): 收到无效 JSON")
            except Exception as e:
                logger.error(f"[DGLabClient] <_receive_loop> (LOG_ERROR): 接收错误: {e}")

    def _handle_received_message(self, data: dict):
        """
        处理接收到的消息，根据消息类型执行不同操作。
        包括绑定结果、错误码、强度/反馈消息、断开指令等。
        """
        msg_type = data.get("type")
        message = data.get("message")

        if msg_type == "bind":
            if message == "200":
                self.target_id = data.get("targetId")
                logger.info(f"[DGLabClient] <_handle_received_message> (LOG_INFO): 绑定成功: target_id = {self.target_id}")
                if self.on_bind_callback:
                    self.on_bind_callback(self.client_id, self.target_id)
            else:
                code = int(message) if message.isdigit() else 0
                err_msg = self._get_error_message(code)
                logger.error(f"[DGLabClient] <_handle_received_message> (LOG_ERROR): 绑定错误: {code} - {err_msg}")
                if self.on_error_callback:
                    self.on_error_callback(code, err_msg)

        elif msg_type == "error":
            code = int(message) if message.isdigit() else 500
            err_msg = self._get_error_message(code)
            logger.error(f"[DGLabClient] <_handle_received_message> (LOG_ERROR): 服务器错误: {code} - {err_msg}")
            if self.on_error_callback:
                self.on_error_callback(code, err_msg)

        elif msg_type == "msg":
            if message.startswith("strength-"):
                logger.info(f"[DGLabClient] <_handle_received_message> (LOG_INFO): 收到强度更新: {message}")
            elif message.startswith("feedback-"):
                logger.info(f"[DGLabClient] <_handle_received_message> (LOG_INFO): 收到反馈: {message}")
            else:
                logger.debug(f"[DGLabClient] <_handle_received_message> (LOG_DEBUG): 收到普通消息: {message}")

        elif msg_type == "break":
            logger.info("[DGLabClient] <_handle_received_message> (LOG_INFO): 收到断开指令，主动断开连接")
            self.is_connected = False

        # 调用用户设置的消息回调
        if self.on_message_callback:
            self.on_message_callback(data)

    # ========== 消息发送 ==========

    async def send(self, data: dict) -> bool:
        """
        发送JSON格式的消息到服务器，并检查消息长度。
        返回 True 表示发送成功。
        """
        if not self.is_connected or not self.websocket:
            logger.warning("[DGLabClient] <send> (LOG_WARN): 未连接，无法发送")
            return False

        json_str = json.dumps(data)
        if len(json_str) > self.config.max_message_length:
            logger.error(f"[DGLabClient] <send> (LOG_ERROR): 消息过长: {len(json_str)} > {self.config.max_message_length}")
            return False

        try:
            await self.websocket.send(json_str)
            logger.debug(f"[DGLabClient] <send> (LOG_DEBUG): 已发送: {json_str}")
            return True
        except Exception as e:
            logger.error(f"[DGLabClient] <send> (LOG_ERROR): 发送失败: {e}")
            return False

    async def send_heartbeat(self) -> bool:
        """发送心跳包到服务器"""
        if not self.client_id:
            logger.debug("[DGLabClient] <send_heartbeat> (LOG_DEBUG): 尚未获得 client_id，跳过心跳")
            return False
        data = {
            "type": "heartbeat",
            "clientId": self.client_id,
            "targetId": self.target_id or "",
            "message": "200"
        }
        return await self.send(data)

    # ========== 设备控制方法 ==========

    async def bind_target(self, target_id: str):
        """绑定目标设备（由 APP 端发起）"""
        if not self.client_id:
            logger.warning("[DGLabClient] <bind_target> (LOG_WARN): 无 client_id，无法绑定")
            return
        self.target_id = target_id
        data = {
            "type": "bind",
            "clientId": self.client_id,
            "targetId": target_id,
            "message": "DGLAB"
        }
        logger.info(f"[DGLabClient] <bind_target> (LOG_INFO): 发送绑定请求: target_id={target_id}")
        await self.send(data)

    def sync_send_strength_operation(self, channel: int, mode: int, value: int) -> bool:
        """同步版本的 send_strength_operation"""
        return self.loop.run_until_complete(self.send_strength_operation(channel, mode, value))

    async def send_strength_operation(self, channel: int, mode: int, value: int) -> bool:
        """
        发送强度控制命令（适配 v2 后端协议）
        mode: 0=减少, 1=增加, 2=设置为指定值
        value: 仅当 mode=2 时有效（0-200），其他模式忽略
        """
        if channel not in (1, 2) or mode not in (0, 1, 2) or not 0 <= value <= 200:
            logger.error(f"[DGLabClient] <send_strength_operation> (LOG_ERROR): 无效参数: channel={channel}, mode={mode}, value={value}")
            return False

        if mode == 2:
            # 设置指定值
            data = {
                "type": 3,
                "channel": channel,
                "strength": value,
                "clientId": self.client_id,
                "targetId": self.target_id,
                "message": "set channel"
            }
            logger.debug(f"[DGLabClient] <send_strength_operation> (LOG_DEBUG): 设置强度: 通道{channel} -> {value}")
        else:
            msg_type = 1 if mode == 0 else 2
            data = {
                "type": msg_type,
                "channel": channel,
                "clientId": self.client_id,
                "targetId": self.target_id,
                "message": "set channel"
            }
            logger.debug(f"[DGLabClient] <send_strength_operation> (LOG_DEBUG): 强度{'减少' if mode==0 else '增加'}: 通道{channel}")
        return await self.send(data)

    def sync_send_pulse(self, channel: str, pulses: list[str], duration: int = 5) -> bool:
        """同步版本的 send_pulse"""
        return self.loop.run_until_complete(self.send_pulse(channel, pulses, duration))

    async def send_pulse(self, channel: str, pulses: list[str], duration: int = 5) -> bool:
        """
        发送波形数据到设备（适配 v2 后端协议）
        channel: 'A' 或 'B'
        pulses: 字符串列表，每个元素为8字节HEX，最多100个
        duration: 波形持续发送时长（秒），服务端会按频率重复发送整个数组
        """
        if channel not in ('A', 'B'):
            logger.error(f"[DGLabClient] <send_pulse> (LOG_ERROR): 无效通道: {channel}，必须为 'A' 或 'B'")
            return False
        if len(pulses) > 100:
            logger.warning("[DGLabClient] <send_pulse> (LOG_WARN): 波形列表超过100个，将被截断")
            pulses = pulses[:100]

        message_content = f"{channel}:{json.dumps(pulses)}"
        data = {
            "type": "clientMsg",
            "channel": channel,
            "time": duration,
            "message": message_content,
            "clientId": self.client_id,
            "targetId": self.target_id
        }
        logger.info(f"[DGLabClient] <send_pulse> (LOG_INFO): 发送波形: 通道={channel}, 脉冲数={len(pulses)}, 持续={duration}秒")
        logger.debug(f"[DGLabClient] <send_pulse> (LOG_DEBUG): 波形数据预览: {message_content[:100]}...")
        return await self.send(data)

    def sync_send_clear_queue(self, channel: int) -> bool:
        """同步版本的 send_clear_queue"""
        return self.loop.run_until_complete(self.send_clear_queue(channel))

    async def send_clear_queue(self, channel: int) -> bool:
        """
        清空指定通道的波形队列（适配 v2 后端协议）
        channel: 1 或 2 (1=A, 2=B)
        """
        if channel not in (1, 2):
            logger.error(f"[DGLabClient] <send_clear_queue> (LOG_ERROR): 无效通道: {channel}")
            return False
        message = f"clear-{channel}"
        data = {
            "type": 4,
            "message": message,
            "clientId": self.client_id,
            "targetId": self.target_id
        }
        logger.info(f"[DGLabClient] <send_clear_queue> (LOG_INFO): 清空队列: 通道{channel}")
        return await self.send(data)

    # ========== 工具方法 ==========

    def generate_qr_content(self) -> Optional[str]:
        """
        生成用于DGLab App扫描的二维码内容。
        格式: https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://<服务器地址>/<客户端ID>
        """
        if not self.client_id:
            logger.error("[DGLabClient] <generate_qr_content> (LOG_ERROR): 无法生成二维码：client_id 为空")
            return None
        server_address = self.config.ws_url.split('://')[1]
        qr = f"https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://{server_address}/{self.client_id}"
        logger.debug(f"[DGLabClient] <generate_qr_content> (LOG_DEBUG): 生成二维码内容: {qr}")
        return qr

    def get_qr(self, filename: Optional[str] = None) -> Optional[str]:
        """
        生成二维码图片并保存到文件，返回文件路径。
        默认文件名: qr_<client_id>.png
        """
        qr_data = self.generate_qr_content()
        if not qr_data:
            return None

        module_dir = os.path.dirname(os.path.abspath(__file__))
        if filename is None:
            safe_client_id = self.client_id.replace('/', '_').replace('\\', '_')
            filename = f"qr_{safe_client_id}.png"
        if not os.path.isabs(filename):
            filepath = os.path.join(module_dir, filename)
        else:
            filepath = filename

        os.makedirs(os.path.dirname(filepath), exist_ok=True)

        try:
            img = qrcode.make(qr_data)
            img.save(filepath)
            logger.info(f"[DGLabClient] <get_qr> (LOG_INFO): 二维码已保存至: {filepath}")
            return filepath
        except Exception as e:
            logger.error(f"[DGLabClient] <get_qr> (LOG_ERROR): 生成二维码失败: {e}")
            return None

    @staticmethod
    def _get_error_message(code: int) -> str:
        """根据错误代码获取对应的错误描述"""
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

    def sync_close(self) -> bool:
        """同步关闭连接"""
        if self.websocket:
            self.loop.run_until_complete(self.close())
        self.is_connected = False
        return True

    async def close(self) -> bool:
        """主动关闭WebSocket连接"""
        if self.websocket:
            await self.websocket.close()
            logger.info("[DGLabClient] <close> (LOG_INFO): WebSocket 连接已关闭")
        self.is_connected = False
        return True
