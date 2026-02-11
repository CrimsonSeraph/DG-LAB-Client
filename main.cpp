#include "DGLABClient.h"
#include "AppConfig.h"
#include "Console.h"
#include <QtWidgets/QApplication>
#include <iostream>

int main(int argc, char *argv[]) {
    Console& console = Console::GetInstance();
    console.Create();

    AppConfig& config = AppConfig::instance();

    // 初始化配置前检查目录
    std::string config_dir = "./config";

    // 尝试手动创建目录
    try {
        std::filesystem::create_directories(config_dir);
        std::cout << "配置目录创建成功: " << config_dir << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "创建配置目录失败: " << e.what() << std::endl;
        // 尝试使用用户目录替代
        const char* home = std::getenv("APPDATA");  // Windows
        if (home) {
            config_dir = std::string(home) + "/DG-LAB-Client/config";
            std::cout << "尝试使用用户目录: " << config_dir << std::endl;
        }
    }

    // 初始化配置
    try {
        if (!config.initialize(config_dir)) {
            std::cerr << "配置系统初始化失败！" << std::endl;
            // 尝试不使用配置文件运行
            std::cout << "将使用默认配置运行..." << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "初始化时发生异常: " << e.what() << std::endl;
        // 继续运行，使用内置默认值
    }

    // 检查是否已初始化
    if (!config.is_initialized()) {
        std::cerr << "配置系统未正确初始化！" << std::endl;
        std::cout << "将以最小配置模式运行..." << std::endl;
    }

    // 然后检查优先级冲突
    std::string error_msg;
    if (config.check_priority_conflict(error_msg)) {
        std::cerr << "优先级冲突: " << error_msg << std::endl;
    }

    // 获取配置
    try {
        std::string app_name = config.get_app_name();
        int log_level = config.get_log_level();
        std::cout << "应用名称: " << app_name << ", 日志级别: " << log_level << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "获取配置失败: " << e.what() << std::endl;
        return -1;
    }

    QApplication app(argc, argv);

    DGLABClient window;
    window.show();

    return app.exec();
}
