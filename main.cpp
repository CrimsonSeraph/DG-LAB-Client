#include "DGLABClient.h"
#include "AppConfig.h"
#include "Console.h"
#include "DebugLog.h"
#include <QtWidgets/QApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // 直接创建控制台，以便在初始化配置系统时输出日志
    Console& console = Console::get_instance();
    console.create();
    // 配置初始化
    auto& config = AppConfig::instance();
    std::string config_dir = "./config";
    try {
        if (!config.initialize(config_dir)) {
            DebugLog::instance().set_log_level("main", LOG_DEBUG);
            LOG_MODULE("main", "main", LOG_WARN, "配置系统初始化失败，使用内存配置");
        }
    }
    catch (const std::exception& e) {
        LOG_MODULE("main", "main", LOG_ERROR, "初始化时发生异常: " << e.what());
    }
    std::string error_msg;
    if (config.check_priority_conflict(error_msg)) {
        LOG_MODULE("main", "main", LOG_WARN, "优先级冲突: " << error_msg);
    }

    // 启用控制台
    bool enable_console = config.get_value<bool>("app.debug", false);
    if (enable_console) {
        Console& console = Console::get_instance();
        if (console.create()) {
            LOG_MODULE("main", "main", LOG_DEBUG, "控制台已启用");
            LOG_MODULE("main", "main", LOG_INFO, "配置初始化完成，debug模式=" << enable_console);
        }
        else {
            LOG_MODULE("main", "main", LOG_WARN, "控制台启用失败（非 Windows 平台不支持）");
        }
    }

    int console_log_level = config.get_value<int>("app.log.console_level", 0);
    DebugLog::instance().set_log_sink_level("console", static_cast<LogLevel>(console_log_level));
    LOG_MODULE("main", "main", LOG_DEBUG, "控制台日志级别设置为: " << console_log_level);
    bool is_only_type_info = config.get_value<bool>("app.log.only_type_info", false);
    DebugLog::instance().set_only_type_info(is_only_type_info);

    // 创建窗口
    DGLABClient window;
    std::string app_name = config.get_value<std::string>("app.name", "DG-LAB-Client");
    std::string app_version = config.get_value<std::string>("app.version", "1.0.0");
    window.setWindowTitle(QString::fromStdString(app_name + "[" + app_version) + "]");
    window.show();
    LOG_MODULE("main", "main", LOG_DEBUG, "窗口已创建，标题: " << window.windowTitle().toStdString());

    return app.exec();
}
