#include "DGLABClient.h"
#include "AppConfig.h"
#include "PyThreadPoolExecutor.h"
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

    // 调用 Py 模型测试
    PyExecutor executor("", false);
    try {
        executor.initialize();

        // 设置 Python 路径
        std::filesystem::path python_dir = std::filesystem::current_path() / "python";
        executor.add_path(python_dir.string());

        executor.import_module("WebSocketCore");
        executor.create_instance("DGLabClient");

        executor.call_sync<void>("set_ws_url", "ws://localhost:9999");
        bool is_connect = executor.call_sync<bool>("connect");
        if (!is_connect) {
            std::cerr << "连接失败" << std::endl;
        }
        else {
            executor.call_sync<void>("send_strength_operation", 1, 2, 10);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "同步调用失败: " << e.what() << std::endl;
    }

    executor.call_sync<void>("close");
    return app.exec();
}
