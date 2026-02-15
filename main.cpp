#include "DGLABClient.h"
#include "AppConfig.h"
#include "PyExecutorManager.h"
#include "Console.h"
#include <QtWidgets/QApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    // 启用控制台
    Console& console = Console::GetInstance();
    console.Create();

    QApplication app(argc, argv);

    // 配置初始化
    AppConfig& config = AppConfig::instance();
    std::string config_dir = "./config";
    try {
        if (!config.initialize(config_dir)) {
            std::cerr << "配置系统初始化失败，使用内存配置" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "初始化时发生异常: " << e.what() << std::endl;
    }

    std::string error_msg;
    if (config.check_priority_conflict(error_msg)) {
        std::cerr << "优先级冲突: " << error_msg << std::endl;
    }

    // 创建窗口
    DGLABClient window;
    std::string app_name = config.get_app_name();
    window.setWindowTitle(QString::fromStdString(app_name));
    window.show();

    // 初始化 Python 解释器
    py::scoped_interpreter guard{};
    PyExecutorManager manager;

    // 设置 Python 路径
    std::filesystem::path python_dir = std::filesystem::current_path() / "python";
    py::module sys = py::module::import("sys");
    sys.attr("path").attr("append")(python_dir.string());

    if (!manager.register_executor("WebSocketCore", "DGLabClient", false)) {
        std::cerr << "执行器注册失败！" << std::endl;
    }
    try {
        manager.call_sync<void>("WebSocketCore", "DGLabClient", "set_ws_url", "ws://localhost:9999");

        bool is_connect = manager.call_sync<bool>("WebSocketCore", "DGLabClient", "connect");
        if (!is_connect) {
            std::cerr << "连接失败" << std::endl;
        }
        else {
            manager.call_sync<void>("WebSocketCore", "DGLabClient", "send_strength_operation", 1, 2, 10);
            std::string server_address = manager.call_sync<std::string>("WebSocketCore", "DGLabClient", "generate_qr_content");
            std::cout << server_address << std::endl;
        }
        manager.call_sync<void>("WebSocketCore", "DGLabClient", "close");
    }
    catch (const std::exception& e) {
        std::cerr << "调用失败: " << e.what() << std::endl;
    }

    return app.exec();
}
