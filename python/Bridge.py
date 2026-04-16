"""
    Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
    SPDX-License-Identifier: GPL-3.0-only
"""

from WebSocketCore import DGLabClient

import asyncio
import json
import logging
import sys

# 配置日志：INFO级别，包含时间、级别、消息
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger("dglab_server")  # 独立的日志记录器


class DGLabServer:
    """
    管理TCP服务器和DGLab客户端。
    负责接收Qt客户端（或其他TCP客户端）的JSON命令，转换为DGLab WebSocket协议并转发。
    """

    def __init__(self):
        self.dglab = DGLabClient()
        self.server = None
        self.qt_client = None

    @staticmethod
    def _normalize_channel(channel):
        """
        将通道参数统一转换为整数（1=A, 2=B）
        支持输入: 1, 2, 'A', 'a', 'B', 'b'
        返回: 1 或 2，无效返回 None
        """
        if isinstance(channel, int):
            return channel if channel in (1, 2) else None
        if isinstance(channel, str):
            upper = channel.upper()
            if upper == 'A':
                return 1
            if upper == 'B':
                return 2
        return None

    @staticmethod
    def _channel_to_str(channel):
        """
        将通道参数统一转换为字符串（'A' 或 'B'）
        支持输入: 1, 2, 'A', 'a', 'B', 'b'
        返回: 'A' 或 'B'，无效返回 None
        """
        if isinstance(channel, int):
            return 'A' if channel == 1 else 'B' if channel == 2 else None
        if isinstance(channel, str):
            upper = channel.upper()
            return upper if upper in ('A', 'B') else None
        return None

    async def handle_qt_client(self, reader, writer):
        """
        处理单个 TCP 客户端连接，循环读取 JSON 命令并响应。
        """
        self.qt_client = writer
        addr = writer.get_extra_info('peername')
        logger.info(f"[DGLabServer] <handle_qt_client> (LOG_INFO): Qt客户端已连接: {addr}")

        try:
            while True:
                # 读取一行（以换行符分隔）
                data = await reader.readline()
                if not data:
                    break  # 客户端关闭连接

                # 解析 JSON
                try:
                    cmd = json.loads(data.decode())
                    logger.debug(f"[DGLabServer] <handle_qt_client> (LOG_DEBUG): 收到命令: {cmd}")
                except json.JSONDecodeError:
                    logger.warning("[DGLabServer] <handle_qt_client> (LOG_WARN): 收到无效 JSON，忽略")
                    await self.send_response(writer, {"status": "error", "message": "Invalid JSON"})
                    continue

                # 处理命令
                try:
                    response = await self.process_command(cmd)
                except Exception as e:
                    logger.exception(f"[DGLabServer] <handle_qt_client> (LOG_ERROR): 处理命令时发生未捕获异常: {e}")
                    response = {"status": "error", "message": f"内部错误: {str(e)}"}
                    if "req_id" in cmd:
                        response["req_id"] = cmd["req_id"]

                # 发送响应
                await self.send_response(writer, response)

        except asyncio.CancelledError:
            logger.debug("[DGLabServer] <handle_qt_client> (LOG_DEBUG): TCP 客户端处理任务被取消")
        finally:
            self.qt_client = None
            writer.close()
            await writer.wait_closed()
            logger.info("[DGLabServer] <handle_qt_client> (LOG_INFO): Qt客户端已断开")

    async def send_response(self, writer, response):
        """发送 JSON 响应，每条消息以换行符结尾，便于 TCP 流解析。"""
        data = json.dumps(response).encode() + b'\n'
        writer.write(data)
        await writer.drain()
        logger.debug(f"[DGLabServer] <send_response> (LOG_DEBUG): 响应已发送: {response}")

    async def process_command(self, cmd):
        """
        解析并执行单个命令，返回响应字典。
        支持的 cmd 类型见各分支。
        """
        req_id = cmd.get("req_id")
        cmd_type = cmd.get("cmd")
        logger.debug(f"[DGLabServer] <process_command> (LOG_DEBUG): 处理命令: {cmd_type}, req_id={req_id}")

        # ---------- 连接相关 ----------
        if cmd_type == "connect":
            logger.info("[DGLabServer] <process_command> (LOG_INFO): 收到连接请求，开始连接 WebSocket 服务器...")
            success = await self.dglab.connect_async()
            response = {
                "status": "ok" if success else "error",
                "message": "连接成功" if success else "连接失败"
            }

        elif cmd_type == "close":
            logger.info("[DGLabServer] <process_command> (LOG_INFO): 收到断开请求，关闭 WebSocket 连接")
            await self.dglab.close()
            response = {"status": "ok", "message": "已断开"}

        elif cmd_type == "set_ws_url":
            url = cmd.get("url")
            if url:
                logger.info(f"[DGLabServer] <process_command> (LOG_INFO): 设置 WebSocket 服务器地址: {url}")
                self.dglab.set_ws_url(url)
                response = {"status": "ok", "message": "设置成功"}
            else:
                logger.warning("[DGLabServer] <process_command> (LOG_WARN): set_ws_url 缺少 url 参数")
                response = {"status": "error", "message": "缺少 url 参数"}

        # ---------- 绑定与状态查询 ----------
        elif cmd_type == "bind_target":
            target_id = cmd.get("target_id")
            if not target_id:
                logger.warning("[DGLabServer] <process_command> (LOG_WARN): bind_target 缺少 target_id 参数")
                response = {"status": "error", "message": "缺少 target_id 参数"}
            else:
                logger.info(f"[DGLabServer] <process_command> (LOG_INFO): 发送绑定请求，target_id={target_id}")
                await self.dglab.bind_target(target_id)
                response = {"status": "ok", "message": "绑定请求已发送"}

        elif cmd_type == "get_client_id":
            client_id = self.dglab.client_id
            logger.debug(f"[DGLabServer] <process_command> (LOG_DEBUG): 返回 client_id: {client_id}")
            response = {"status": "ok", "message": client_id if client_id else ""}

        elif cmd_type == "get_target_id":
            target_id = self.dglab.target_id
            logger.debug(f"[DGLabServer] <process_command> (LOG_DEBUG): 返回 target_id: {target_id}")
            response = {"status": "ok", "message": target_id if target_id else ""}

        elif cmd_type == "get_connection_status":
            status = {
                "connected": self.dglab.is_connected,
                "has_client_id": self.dglab.client_id is not None,
                "has_target_id": self.dglab.target_id is not None
            }
            logger.debug(f"[DGLabServer] <process_command> (LOG_DEBUG): 连接状态: {status}")
            response = {"status": "ok", **status}

        # ---------- 强度控制 ----------
        elif cmd_type == "send_strength":
            raw_channel = cmd.get("channel")
            mode = cmd.get("mode")  # 0=减少,1=增加,2=设置指定值,3=连续减少,4=连续增加
            value = cmd.get("value", 0) # mode=2时为目标强度(0-200); mode=3/4时为连续次数

            if raw_channel is None or mode is None:
                logger.warning("[DGLabServer] <process_command> (LOG_WARN): send_strength 缺少 channel 或 mode 参数")
                response = {"status": "error", "message": "缺少 channel 或 mode 参数"}
            else:
                channel = self._normalize_channel(raw_channel)
                if channel is None:
                    logger.warning(f"[DGLabServer] <process_command> (LOG_WARN): 无效的 channel 值: {raw_channel}")
                    response = {"status": "error", "message": "channel 必须为 1/2 或 'A'/'B'"}
                elif mode not in (0, 1, 2, 3, 4):
                    logger.warning(f"[DGLabServer] <process_command> (LOG_WARN): 无效的 mode 值: {mode}")
                    response = {"status": "error", "message": "mode 必须为 0-4"}
                else:
                    # 连续增减 (mode=3 减少, mode=4 增加)
                    if mode == 3 or mode == 4:
                        repeat = int(value) if value > 0 else 1
                        if repeat > 100:
                            repeat = 100
                            logger.warning("[DGLabServer] <process_command> (LOG_WARN): 连续次数超过100，已限制为100")
                        base_mode = 0 if mode == 3 else 1   # 0=减少,1=增加
                        logger.info(f"[DGLabServer] <process_command> (LOG_INFO): 连续{'减少' if base_mode==0 else '增加'} {repeat} 次 (通道{channel})")
                        success = True
                        for i in range(repeat):
                            ok = await self.dglab.send_strength_operation(channel, base_mode, 1)
                            if not ok:
                                success = False
                                logger.error(f"[DGLabServer] <process_command> (LOG_ERROR): 第{i+1}次发送失败")
                                break
                            if (i + 1) % 10 == 0:
                                logger.debug(f"[DGLabServer] <process_command> (LOG_DEBUG): 已执行 {i+1}/{repeat} 次")
                        response = {
                            "status": "ok" if success else "partial",
                            "message": f"已执行 {repeat} 次{'减少' if base_mode==0 else '增加'}" if success else "部分失败"
                        }
                    else:
                        # 单次操作 (0,1,2)
                        logger.info(f"[DGLabServer] <process_command> (LOG_INFO): 强度操作: 通道={channel}, mode={mode}, value={value}")
                        success = await self.dglab.send_strength_operation(channel, mode, value)
                        response = {
                            "status": "ok" if success else "error",
                            "message": "强度指令已发送" if success else "发送失败"
                        }

        # ---------- 波形发送 ----------
        elif cmd_type == "send_pulse":
            raw_channel = cmd.get("channel")
            pulses = cmd.get("pulses")
            duration = cmd.get("duration", 5)

            if not raw_channel or not pulses:
                logger.warning("[DGLabServer] <process_command> (LOG_WARN): send_pulse 缺少 channel 或 pulses 参数")
                response = {"status": "error", "message": "缺少 channel 或 pulses 参数"}
            elif not isinstance(pulses, list) or len(pulses) == 0:
                logger.warning("[DGLabServer] <process_command> (LOG_WARN): pulses 不是非空列表")
                response = {"status": "error", "message": "pulses 必须为非空列表"}
            else:
                channel = self._channel_to_str(raw_channel)
                if channel is None:
                    logger.warning(f"[DGLabServer] <process_command> (LOG_WARN): 无效的 channel 值: {raw_channel}")
                    response = {"status": "error", "message": "channel 必须为 1/2 或 'A'/'B'"}
                else:
                    logger.info(f"[DGLabServer] <process_command> (LOG_INFO): 发送波形: 通道={channel}, 脉冲数={len(pulses)}, 持续={duration}秒")
                    success = await self.dglab.send_pulse(channel, pulses, duration)
                    response = {
                        "status": "ok" if success else "error",
                        "message": "波形已发送" if success else "发送失败"
                    }

        # ---------- 清空队列 ----------
        elif cmd_type == "clear_queue":
            raw_channel = cmd.get("channel")
            if raw_channel is None:
                logger.warning("[DGLabServer] <process_command> (LOG_WARN): clear_queue 缺少 channel 参数")
                response = {"status": "error", "message": "缺少 channel 参数"}
            else:
                channel = self._normalize_channel(raw_channel)
                if channel is None:
                    logger.warning(f"[DGLabServer] <process_command> (LOG_WARN): 无效的 channel 值: {raw_channel}")
                    response = {"status": "error", "message": "channel 必须为 1/2 或 'A'/'B'"}
                else:
                    logger.info(f"[DGLabServer] <process_command> (LOG_INFO): 清空队列: 通道={channel}")
                    success = await self.dglab.send_clear_queue(channel)
                    response = {
                        "status": "ok" if success else "error",
                        "message": "清空指令已发送" if success else "发送失败"
                    }

        # ---------- 二维码 ----------
        elif cmd_type == "get_qr_path":
            qr_path = self.dglab.get_qr()
            logger.debug(f"[DGLabServer] <process_command> (LOG_DEBUG): 生成二维码: {qr_path}")
            response = {"status": "ok", "message": qr_path if qr_path else ""}

        # ---------- 设置日志级别 ----------
        elif cmd_type == "set_log_level":
            level_str = cmd.get("level", "").upper()
            valid_levels = {"DEBUG": logging.DEBUG, "INFO": logging.INFO,
                        "WARNING": logging.WARNING, "ERROR": logging.ERROR}
            if level_str in valid_levels:
                # 设置 dglab_server 的日志级别
                logger.setLevel(valid_levels[level_str])
                # 同时设置 DGLabClient 的日志级别
                logging.getLogger("dglab_client").setLevel(valid_levels[level_str])
                # 设置根日志器，确保所有模块生效
                logging.getLogger().setLevel(valid_levels[level_str])
                response = {"status": "ok", "message": f"日志级别已设置为 {level_str}"}
                logger.info(f"日志级别已动态修改为 {level_str}")
            else:
                response = {"status": "error", "message": f"无效的日志级别: {level_str}"}

        else:
            logger.warning(f"[DGLabServer] <process_command> (LOG_WARN): 未知命令类型: {cmd_type}")
            response = {"status": "error", "message": f"未知命令: {cmd_type}"}

        # 透传 req_id
        if req_id is not None:
            response["req_id"] = req_id

        return response

    async def start_server(self):
        """
        启动 TCP 服务器，监听 127.0.0.1 的随机端口，
        并将端口号打印到 stdout（供父进程获取）。
        """
        self.server = await asyncio.start_server(
            self.handle_qt_client, host='127.0.0.1', port=0
        )
        port = self.server.sockets[0].getsockname()[1]
        print(port, flush=True)
        logger.info(f"[DGLabServer] <start_server> (LOG_INFO): TCP服务器启动，端口: {port}")
        async with self.server:
            await self.server.serve_forever()


async def main():
    server = DGLabServer()
    await server.start_server()


if __name__ == "__main__":
    asyncio.run(main())
