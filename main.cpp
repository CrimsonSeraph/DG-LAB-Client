#include "DGLABClient.h"
#include "AppConfig.h"
#include "Console.h"
#include <QtWidgets/QApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    Console& console = Console::GetInstance();
    console.Create();

    // 先创建 QApplication
    QApplication app(argc, argv);

    // 然后初始化配置
    AppConfig& config = AppConfig::instance();

    // 使用默认配置目录
    std::string config_dir = "./config";

    // 初始化配置
    try {
        if (!config.initialize(config_dir)) {
            std::cerr << "配置系统初始化失败，使用内存配置" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "初始化时发生异常: " << e.what() << std::endl;
        // 继续运行，使用内置默认值
    }

    // 检查优先级冲突
    std::string error_msg;
    if (config.check_priority_conflict(error_msg)) {
        std::cerr << "优先级冲突: " << error_msg << std::endl;
    }

    std::string app_name = config.get_app_name();
    std::cout << app_name << std::endl;

    DGLABClient window;
    window.show();

    return app.exec();
}
