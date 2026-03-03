import asyncio
import json
import logging
import sys
from WebSocketCore import DGLabClient

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger("dglab_server")

class DGLabServer:
    """管理TCP服务器和DGLab客户端"""
    def __init__(self):
        self.dglab = DGLabClient()
        self.server = None
        self.qt_client = None

    async def handle_qt_client(self, reader, writer):
        self.qt_client = writer
        addr = writer.get_extra_info('peername')
        logger.info(f"Qt客户端已连接: {addr}")
        try:
            while True:
                data = await reader.read(4096)
                if not data:
                    break
                try:
                    cmd = json.loads(data.decode())
                except json.JSONDecodeError:
                    await self.send_response(writer, {"status": "error", "message": "Invalid JSON"})
                    continue

                try:
                    response = await self.process_command(cmd)
                except Exception as e:
                    logger.exception(f"处理命令时发生未捕获异常: {e}")
                    response = {"status": "error", "message": f"内部错误: {str(e)}"}
                    if "req_id" in cmd:
                        response["req_id"] = cmd["req_id"]

                await self.send_response(writer, response)
        except asyncio.CancelledError:
            pass
        finally:
            self.qt_client = None
            writer.close()
            await writer.wait_closed()
            logger.info("Qt客户端已断开")

    async def send_response(self, writer, response):
        """发送JSON响应（添加换行符作为消息边界）"""
        data = json.dumps(response).encode() + b'\n'
        writer.write(data)
        await writer.drain()

    async def process_command(self, cmd):
        """处理单个命令，返回响应字典"""
        req_id = cmd.get("req_id")
        cmd_type = cmd.get("cmd")
        if cmd_type == "connect":
            success = await self.dglab.connect_async()
            response = {"status": "ok" if success else "error", "message": "连接成功" if success else "连接失败"}

        elif cmd_type == "close":
            await self.dglab.close()
            response = {"status": "ok", "message": "已断开"}

        elif cmd_type == "set_ws_url":
            post = cmd.get("post")
            self.dglab.set_ws_url(f"ws://localhost:{post}")
            response = {"status": "ok", "message": "设置成功"}

        elif cmd_type == "bind_target":
            target_id = cmd.get("target_id")
            await self.dglab.bind_target(target_id)
            response = {"status": "ok", "message": "绑定请求已发送"}

        elif cmd_type == "send_strength":
            channel = cmd.get("channel")
            mode = cmd.get("mode")
            value = cmd.get("value")
            success = await self.dglab.send_strength_operation(channel, mode, value)
            response = {"status": "ok" if success else "error", "message": "强度指令已发送" if success else "发送失败"}

        elif cmd_type == "get_qr":
            qr = self.dglab.generate_qr_content()
            response = {"status": "ok", "qr": qr if qr else ""}

        else:
            response = {"status": "error", "message": f"未知命令: {cmd_type}"}

        if req_id is not None:
            response["req_id"] = req_id

        return response

    async def start_server(self):
        """启动TCP服务器，监听随机端口，并将端口号打印到stdout"""
        self.server = await asyncio.start_server(
            self.handle_qt_client, host='127.0.0.1', port=0
        )
        port = self.server.sockets[0].getsockname()[1]
        print(port, flush=True)
        logger.info(f"TCP服务器启动，端口: {port}")
        async with self.server:
            await self.server.serve_forever()

async def main():
    server = DGLabServer()
    await server.start_server()

if __name__ == "__main__":
    asyncio.run(main())
